import {
  HAZARD_ELEVATED,
  HAZARD_NO_CLAIM,
  PLANE_CM_NONE,
  ROUTE_NONE,
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

export function imageHeuristicClaim(input) {
  const ready = pipelineReady(input);
  return {
    source: SOURCE_IMAGE_HEURISTIC,
    hazard: ready && input.detectionEnabled && input.zoneHit ? HAZARD_ELEVATED : HAZARD_NO_CLAIM,
    unavailable: ready ? 0 : 1,
    route: ROUTE_NONE,
    planeCm: PLANE_CM_NONE,
    sceneDepth: 0,
    tracking: ready ? TRACKING_NORMAL : TRACKING_UNAVAILABLE,
    experimental: true,
  };
}

export function manualClaim(input) {
  let route = input.route;
  if (route !== 0 && route !== 1 && route !== 2) {
    route = ROUTE_NONE;
  }
  return {
    source: SOURCE_SIMULATION,
    hazard: input.hazard ? HAZARD_ELEVATED : HAZARD_NO_CLAIM,
    unavailable: input.unavailable ? 1 : 0,
    route,
    planeCm: PLANE_CM_NONE,
    sceneDepth: 0,
    tracking: input.tabHidden ? TRACKING_UNAVAILABLE : TRACKING_NORMAL,
    experimental: false,
  };
}

export function claimForMode(input) {
  if (input.mode === "manual" && !input.tabHidden) {
    return manualClaim(input);
  }
  return imageHeuristicClaim(input);
}
