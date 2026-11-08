import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  PLANE_CM_NONE,
  SIDE_LEFT,
  SIDE_RIGHT,
  SOURCE_CAMERA,
  SOURCE_IMAGE_HEURISTIC,
  SOURCE_SIMULATION,
  TRACKING_NORMAL,
  commandAdmissible,
  crc16,
  createWriteQueue,
  encodeCommand,
} from "./protocol.js";
import { claimForMode, imageHeuristicClaim } from "./state.js";
import { iou, sideWarnings, updateTracks } from "./tracking.js";

function box(x, y, w, h) {
  return { x, y, w, h };
}

function track(id, className, score, bbox) {
  return { id, className, score, bbox, lastSeen: 0 };
}

test("crc16 matches the CCITT-FALSE vector", () => {
  assert.equal(crc16(new TextEncoder().encode("123456789")), 0x29b1);
});

test("javascript packets match the C encoder", () => {
  const fixture = JSON.parse(
    readFileSync(new URL("../prototype/build/packets.json", import.meta.url), "utf8")
  );
  const shared = {
    unavailable: 0,
    tracking: TRACKING_NORMAL,
    sceneDepth: 0,
    sequence: 9,
    timeMsLow: 200,
    planeCm: PLANE_CM_NONE,
  };
  const expected = {
    left: { ...shared, source: SOURCE_SIMULATION, sides: SIDE_LEFT },
    both: { ...shared, source: SOURCE_IMAGE_HEURISTIC, sides: SIDE_LEFT | SIDE_RIGHT },
    none: { ...shared, source: SOURCE_CAMERA, sides: 0, unavailable: 1 },
  };
  for (const row of fixture) {
    const encoded = encodeCommand(expected[row.name]);
    assert.ok(encoded, row.name);
    assert.deepEqual(Array.from(encoded), row.bytes, row.name);
    assert.equal(encoded[0], 2);
    assert.equal(encoded[4], 0);
  }
});

test("version 2 rejects a camera side and a fake distance", () => {
  assert.equal(
    commandAdmissible({
      source: SOURCE_CAMERA,
      sides: SIDE_LEFT,
      unavailable: 0,
      tracking: TRACKING_NORMAL,
      planeCm: PLANE_CM_NONE,
    }),
    false
  );
  assert.equal(
    encodeCommand({
      source: SOURCE_IMAGE_HEURISTIC,
      sides: SIDE_RIGHT,
      unavailable: 0,
      tracking: TRACKING_NORMAL,
      sceneDepth: 0,
      sequence: 1,
      timeMsLow: 1,
      planeCm: 40,
    }),
    null
  );
});

test("center gate chooses left, right, both, or none", () => {
  const tracks = [
    track(1, "person", 0.9, box(20, 40, 20, 20)),
    track(2, "car", 0.8, box(60, 40, 20, 20)),
    track(3, "person", 0.9, box(0, 0, 10, 10)),
    track(4, "kite", 0.99, box(40, 40, 20, 20)),
  ];
  const both = sideWarnings(tracks, 100, 100, 0);
  assert.equal(both.left, true);
  assert.equal(both.right, true);
  assert.equal(both.onDivider, false);

  const leftOnly = sideWarnings([track(1, "person", 0.9, box(20, 40, 20, 20))], 100, 100, 0);
  assert.equal(leftOnly.left, true);
  assert.equal(leftOnly.right, false);

  const divider = sideWarnings([track(1, "bus", 0.9, box(40, 40, 20, 20))], 100, 100, 0);
  assert.equal(divider.left, true);
  assert.equal(divider.right, true);
  assert.equal(divider.onDivider, true);
  assert.equal(divider.hits[0].side, "divider");

  const none = sideWarnings([track(3, "person", 0.9, box(0, 0, 10, 10))], 100, 100, 0);
  assert.equal(none.left, false);
  assert.equal(none.right, false);
});

test("iou keeps an id and expiry drops it", () => {
  const first = updateTracks([], [{ className: "person", score: 0.9, bbox: box(0, 0, 10, 10) }], 1000);
  const second = updateTracks(first, [{ className: "person", score: 0.8, bbox: box(1, 1, 10, 10) }], 1100);
  assert.equal(second[0].id, first[0].id);
  assert.ok(iou(box(0, 0, 10, 10), box(1, 1, 10, 10)) > 0.3);
  assert.equal(updateTracks(second, [], 2000).length, 0);
});

test("faults clear sides and stay distinct from a manual side", () => {
  const fresh = imageHeuristicClaim({
    cameraOn: true,
    modelReady: true,
    modelError: false,
    frameAgeMs: 100,
    tabHidden: false,
    detectionEnabled: true,
    sides: SIDE_LEFT,
  });
  assert.equal(fresh.sides, SIDE_LEFT);
  assert.equal(fresh.unavailable, 0);
  assert.equal(fresh.planeCm, PLANE_CM_NONE);

  const stale = imageHeuristicClaim({
    cameraOn: false,
    modelReady: true,
    modelError: false,
    frameAgeMs: 10,
    tabHidden: false,
    detectionEnabled: true,
    sides: SIDE_RIGHT,
  });
  assert.equal(stale.sides, 0);
  assert.equal(stale.unavailable, 1);

  const hidden = claimForMode({
    mode: "manual",
    tabHidden: true,
    cameraOn: false,
    modelReady: false,
    modelError: false,
    frameAgeMs: null,
    detectionEnabled: false,
    sides: 0,
    left: true,
    right: false,
  });
  assert.equal(hidden.source, SOURCE_IMAGE_HEURISTIC);
  assert.equal(hidden.sides, 0);

  const manual = claimForMode({
    mode: "manual",
    tabHidden: false,
    left: true,
    right: false,
  });
  assert.equal(manual.source, SOURCE_SIMULATION);
  assert.equal(manual.sides, SIDE_LEFT);
  assert.equal(manual.unavailable, 0);
});

test("writes stay in order", async () => {
  const seen = [];
  let active = 0;
  let maxActive = 0;
  const queue = createWriteQueue(async (value) => {
    active += 1;
    maxActive = Math.max(maxActive, active);
    await new Promise((resolve) => setTimeout(resolve, 5));
    seen.push(value);
    active -= 1;
  });
  await Promise.all([queue.enqueue("a"), queue.enqueue("b"), queue.enqueue("c")]);
  assert.deepEqual(seen, ["a", "b", "c"]);
  assert.equal(maxActive, 1);
});
