import {
  PLANE_CM_NONE,
  SIDE_LEFT,
  SIDE_RIGHT,
  SOURCE_IMAGE_HEURISTIC,
  SOURCE_SIMULATION,
  STALE_MS,
  TRACKING_NORMAL,
  TRACKING_UNAVAILABLE,
} from "./protocol.js";

export function pipelineReady(input) {
  if (!input.cameraOn || !input.modelReady || input.modelError || input.tabHidden) {
    return false;
  }
  if (input.frameAgeMs === null || input.frameAgeMs === undefined || input.frameAgeMs > STALE_MS) {
    return false;
  }
  return true;
}

function cleanSides(sides) {
  return sides & (SIDE_LEFT | SIDE_RIGHT);
}

export function imageHeuristicClaim(input) {
  const ready = pipelineReady(input);
  return {
    source: SOURCE_IMAGE_HEURISTIC,
    sides: ready && input.detectionEnabled ? cleanSides(input.sides) : 0,
    unavailable: ready ? 0 : 1,
    planeCm: PLANE_CM_NONE,
    sceneDepth: 0,
    tracking: ready ? TRACKING_NORMAL : TRACKING_UNAVAILABLE,
    experimental: true,
  };
}

export function manualClaim(input) {
  let sides = 0;
  if (input.left) {
    sides |= SIDE_LEFT;
  }
  if (input.right) {
    sides |= SIDE_RIGHT;
  }
  return {
    source: SOURCE_SIMULATION,
    sides,
    unavailable: 0,
    planeCm: PLANE_CM_NONE,
    sceneDepth: 0,
    tracking: TRACKING_NORMAL,
    experimental: false,
  };
}

export function claimForMode(input) {
  if (input.mode === "manual" && !input.tabHidden) {
    return manualClaim(input);
  }
  return imageHeuristicClaim(input);
}
