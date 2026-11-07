import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  HAZARD_ELEVATED,
  HAZARD_NO_CLAIM,
  PLANE_CM_NONE,
  ROUTE_DEMO_LEFT,
  ROUTE_NONE,
  SOURCE_CAMERA,
  SOURCE_IMAGE_HEURISTIC,
  SOURCE_SIMULATION,
  TRACKING_NORMAL,
  commandAdmissible,
  crc16,
  createWriteQueue,
  encodeCommand,
} from "./protocol.js";
import { claimForMode, imageHeuristicClaim, pipelineReady } from "./state.js";
import { centerZoneHit, iou, updateTracks } from "./tracking.js";

function box(x, y, w, h) {
  return { x, y, w, h };
}

test("crc16 matches the CCITT-FALSE vector", () => {
  const digits = new TextEncoder().encode("123456789");
  assert.equal(crc16(digits), 0x29b1);
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
    simulation: {
      ...shared,
      source: SOURCE_SIMULATION,
      hazard: HAZARD_ELEVATED,
      route: ROUTE_DEMO_LEFT,
    },
    image: {
      ...shared,
      source: SOURCE_IMAGE_HEURISTIC,
      hazard: HAZARD_ELEVATED,
      route: ROUTE_NONE,
    },
    camera: {
      ...shared,
      source: SOURCE_CAMERA,
      hazard: HAZARD_NO_CLAIM,
      unavailable: 1,
      route: ROUTE_NONE,
    },
  };
  for (const row of fixture) {
    const encoded = encodeCommand(expected[row.name]);
    assert.ok(encoded, row.name);
    assert.deepEqual(Array.from(encoded), row.bytes, row.name);
  }
});

test("image heuristic cannot carry a distance or a route", () => {
  assert.equal(
    commandAdmissible({
      source: SOURCE_IMAGE_HEURISTIC,
      hazard: HAZARD_ELEVATED,
      unavailable: 0,
      route: ROUTE_DEMO_LEFT,
      tracking: TRACKING_NORMAL,
      planeCm: PLANE_CM_NONE,
    }),
    false
  );
  assert.equal(
    encodeCommand({
      source: SOURCE_IMAGE_HEURISTIC,
      hazard: HAZARD_ELEVATED,
      unavailable: 0,
      route: ROUTE_NONE,
      tracking: TRACKING_NORMAL,
      sceneDepth: 0,
      sequence: 1,
      timeMsLow: 1,
      planeCm: 40,
    }),
    null
  );
  assert.equal(
    commandAdmissible({
      source: SOURCE_CAMERA,
      hazard: HAZARD_ELEVATED,
      unavailable: 0,
      route: ROUTE_NONE,
      tracking: TRACKING_NORMAL,
      planeCm: PLANE_CM_NONE,
    }),
    false
  );
});

test("iou keeps an id and expiry drops it", () => {
  const first = updateTracks(
    [],
    [{ className: "person", score: 0.9, bbox: box(0, 0, 10, 10) }],
    1000
  );
  const second = updateTracks(
    first,
    [{ className: "person", score: 0.8, bbox: box(1, 1, 10, 10) }],
    1100
  );
  assert.equal(second.length, 1);
  assert.equal(second[0].id, first[0].id);
  assert.ok(iou(box(0, 0, 10, 10), box(1, 1, 10, 10)) > 0.3);
  const moved = updateTracks(
    second,
    [{ className: "person", score: 0.8, bbox: box(100, 100, 10, 10) }],
    1200
  );
  assert.notEqual(moved[0].id, second[0].id);
  const expired = updateTracks(second, [], 2000);
  assert.equal(expired.length, 0);
});

test("center warning is 2D and ignores unsupported classes", () => {
  const tracks = [
    { id: 1, className: "kite", score: 0.99, bbox: box(40, 40, 20, 20), lastSeen: 0 },
    { id: 2, className: "person", score: 0.2, bbox: box(40, 40, 20, 20), lastSeen: 0 },
    { id: 3, className: "person", score: 0.8, bbox: box(0, 0, 10, 10), lastSeen: 0 },
    { id: 4, className: "car", score: 0.7, bbox: box(40, 40, 20, 20), lastSeen: 0 },
  ];
  const hit = centerZoneHit(tracks, 100, 100, 0);
  assert.equal(hit.className, "car");
  assert.equal(hit.zone, "center");
  assert.equal(centerZoneHit([], 100, 100, 0), null);
});

test("faults clear an image hazard and stay distinct from manual input", () => {
  const fresh = imageHeuristicClaim({
    cameraOn: true,
    modelReady: true,
    modelError: false,
    frameAgeMs: 100,
    tabHidden: false,
    detectionEnabled: true,
    zoneHit: true,
  });
  assert.equal(fresh.source, SOURCE_IMAGE_HEURISTIC);
  assert.equal(fresh.hazard, HAZARD_ELEVATED);
  assert.equal(fresh.planeCm, PLANE_CM_NONE);
  assert.equal(fresh.route, ROUTE_NONE);

  const stale = imageHeuristicClaim({
    cameraOn: true,
    modelReady: true,
    modelError: false,
    frameAgeMs: 1000,
    tabHidden: false,
    detectionEnabled: true,
    zoneHit: true,
  });
  assert.equal(stale.hazard, HAZARD_NO_CLAIM);
  assert.equal(stale.unavailable, 1);
  assert.equal(pipelineReady({
    cameraOn: false,
    modelReady: true,
    modelError: false,
    frameAgeMs: 10,
    tabHidden: false,
  }), false);

  const hiddenManual = claimForMode({
    mode: "manual",
    tabHidden: true,
    cameraOn: false,
    modelReady: false,
    modelError: false,
    frameAgeMs: null,
    detectionEnabled: false,
    zoneHit: false,
    hazard: true,
    unavailable: false,
    route: ROUTE_DEMO_LEFT,
  });
  assert.equal(hiddenManual.source, SOURCE_IMAGE_HEURISTIC);
  assert.equal(hiddenManual.hazard, HAZARD_NO_CLAIM);
  assert.notEqual(hiddenManual.source, SOURCE_SIMULATION);

  const manual = claimForMode({
    mode: "manual",
    tabHidden: false,
    hazard: true,
    unavailable: false,
    route: ROUTE_DEMO_LEFT,
  });
  assert.equal(manual.source, SOURCE_SIMULATION);
  assert.equal(manual.hazard, HAZARD_ELEVATED);
  assert.equal(manual.route, ROUTE_DEMO_LEFT);
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
