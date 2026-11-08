import {
  COMMAND_UUID,
  HAZARD_ELEVATED,
  HEARTBEAT_MS,
  SERVICE_UUID,
  SOURCE_IMAGE_HEURISTIC,
  SOURCE_SIMULATION,
  STATUS_UUID,
  createWriteQueue,
  decodeStatus,
  encodeCommand,
} from "./protocol.js";
import { claimForMode } from "./state.js";
import { centerZoneHit, updateTracks } from "./tracking.js";

const TFJS_URL = "https://cdn.jsdelivr.net/npm/@tensorflow/tfjs@4.22.0/dist/tf.min.js";
const COCO_URL = "https://cdn.jsdelivr.net/npm/@tensorflow-models/coco-ssd@2.2.3/dist/coco-ssd.min.js";
const MODEL_BASE = "lite_mobilenet_v2";

const diagnostics = document.querySelector("#diagnostics");
const scriptState = document.querySelector("#script-state");
const orientationText = document.querySelector("#orientation");
const cameraStatus = document.querySelector("#camera-status");
const modelStatus = document.querySelector("#model-status");
const detectStatus = document.querySelector("#detect-status");
const bleStatus = document.querySelector("#ble-status");
const boardStatus = document.querySelector("#board-status");
const claimStatus = document.querySelector("#claim-status");
const video = document.querySelector("#video");
const overlay = document.querySelector("#overlay");
const manualMode = document.querySelector("#manual-mode");
const manualHazard = document.querySelector("#manual-hazard");
const manualUnavailable = document.querySelector("#manual-unavailable");
const manualRoute = document.querySelector("#manual-route");
const detectEnable = document.querySelector("#detect-enable");

const state = {
  cameraOn: false,
  detecting: false,
  model: null,
  modelReady: false,
  modelError: false,
  lastFrameMs: null,
  tracks: [],
  zoneHit: null,
  device: null,
  server: null,
  commandChar: null,
  queue: null,
  reconnectsLeft: 0,
  sequence: 0,
  stream: null,
};

scriptState.textContent = "Page script started. Camera inference on Bluefy has not been tried from this repository.";

function setBox(element, text, kind) {
  element.textContent = text;
  element.className = kind;
}

function diagnosticsText() {
  const lines = [
    window.isSecureContext
      ? "This page is a secure context."
      : "This page is not a secure context. Camera and Bluetooth need HTTPS.",
    navigator.bluetooth
      ? "Web Bluetooth is present. That does not prove Bluefy can use it together with the camera."
      : "Web Bluetooth is missing. Bluefy is expected to provide it; this page cannot add it.",
    navigator.mediaDevices && navigator.mediaDevices.getUserMedia
      ? "Camera API is present."
      : "Camera API is missing.",
  ];
  setBox(diagnostics, lines.join(" "), window.isSecureContext ? "ok" : "bad");
}

function sourceName(source) {
  if (source === SOURCE_SIMULATION) {
    return "manual simulation";
  }
  if (source === SOURCE_IMAGE_HEURISTIC) {
    return "image heuristic";
  }
  return "other";
}

function currentClaim() {
  const frameAgeMs = state.lastFrameMs === null ? null : performance.now() - state.lastFrameMs;
  const mode = detectEnable.checked ? "detect" : manualMode.checked ? "manual" : "idle";
  return claimForMode({
    mode,
    tabHidden: document.visibilityState === "hidden",
    cameraOn: state.cameraOn,
    modelReady: state.modelReady,
    modelError: state.modelError,
    frameAgeMs,
    detectionEnabled: detectEnable.checked,
    zoneHit: Boolean(state.zoneHit),
    hazard: manualHazard.checked,
    unavailable: manualUnavailable.checked,
    route: Number(manualRoute.value),
  });
}

function describeClaim(claim) {
  const hazard = claim.hazard === HAZARD_ELEVATED ? "elevated image or manual hazard" : "no hazard claim";
  const availability = claim.unavailable ? "unavailable" : "pipeline running";
  const hit = state.zoneHit
    ? `Center box: ${state.zoneHit.className} score ${state.zoneHit.score.toFixed(2)} id ${state.zoneHit.trackId}.`
    : "No supported class in the center zone.";
  claimStatus.textContent = `${sourceName(claim.source)}. ${hazard}. ${availability}. ${hit} This is not an all-clear.`;
}

async function sendClaim() {
  const claim = currentClaim();
  describeClaim(claim);
  if (!state.queue || !state.commandChar) {
    return;
  }
  state.sequence = (state.sequence + 1) >>> 0;
  const bytes = encodeCommand({
    source: claim.source,
    hazard: claim.hazard,
    unavailable: claim.unavailable,
    route: claim.route,
    tracking: claim.tracking,
    sceneDepth: 0,
    sequence: state.sequence,
    timeMsLow: Date.now() & 0xffff,
    planeCm: claim.planeCm,
  });
  if (!bytes) {
    setBox(bleStatus, "The shared packet rules refused this claim. Nothing was sent.", "bad");
    return;
  }
  try {
    await state.queue.enqueue(bytes);
  } catch (error) {
    setBox(bleStatus, `Bluetooth write failed: ${error.message}`, "bad");
  }
}

function drawTracks() {
  const width = video.videoWidth;
  const height = video.videoHeight;
  if (!width || !height) {
    return;
  }
  overlay.width = width;
  overlay.height = height;
  const context = overlay.getContext("2d");
  context.clearRect(0, 0, width, height);
  context.strokeStyle = "#ffe14a";
  context.lineWidth = 3;
  context.font = "20px sans-serif";
  context.fillStyle = "#ffe14a";
  for (const track of state.tracks) {
    context.strokeRect(track.bbox.x, track.bbox.y, track.bbox.w, track.bbox.h);
    context.fillText(`${track.id} ${track.className} ${track.score.toFixed(2)}`, track.bbox.x, track.bbox.y + 22);
  }
  const x = width * 0.25;
  const y = height * 0.25;
  context.strokeStyle = "#ffffff";
  context.strokeRect(x, y, width * 0.5, height * 0.5);
}

let detectTimer = 0;
async function detectOnce() {
  if (!state.model || !state.cameraOn || state.modelError) {
    return;
  }
  try {
    const predictions = await state.model.detect(video, 20, 0.4);
    const now = performance.now();
    const detections = predictions.map((prediction) => ({
      className: prediction.class,
      score: prediction.score,
      bbox: {
        x: prediction.bbox[0],
        y: prediction.bbox[1],
        w: prediction.bbox[2],
        h: prediction.bbox[3],
      },
    }));
    state.tracks = updateTracks(state.tracks, detections, now);
    state.lastFrameMs = now;
    state.zoneHit = centerZoneHit(state.tracks, video.videoWidth, video.videoHeight, now);
    drawTracks();
    detectStatus.textContent = state.zoneHit
      ? `Experimental center warning: ${state.zoneHit.className}, score ${state.zoneHit.score.toFixed(2)}, track ${state.zoneHit.trackId}. Not a distance.`
      : "No supported class in the center zone. That is not an all-clear.";
  } catch (error) {
    state.modelError = true;
    state.zoneHit = null;
    setBox(modelStatus, `Model stopped: ${error.message}. No brightness substitute is used.`, "bad");
  }
}

function detectionLoop(now) {
  if (state.cameraOn && state.modelReady && !state.modelError && !state.detecting && now - detectTimer > 200) {
    detectTimer = now;
    state.detecting = true;
    detectOnce().finally(() => {
      state.detecting = false;
    });
  }
  if (state.cameraOn) {
    requestAnimationFrame(detectionLoop);
  }
}

async function loadScript(url) {
  await new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = url;
    script.onload = resolve;
    script.onerror = () => reject(new Error(`Could not download ${url}`));
    document.head.appendChild(script);
  });
}

document.querySelector("#camera-start").addEventListener("click", async () => {
  try {
    const stream = await navigator.mediaDevices.getUserMedia({
      audio: false,
      video: { facingMode: { ideal: "environment" } },
    });
    state.stream = stream;
    video.srcObject = stream;
    state.cameraOn = true;
    state.lastFrameMs = null;
    setBox(cameraStatus, "Rear camera is on. The picture stays in this browser.", "ok");
    requestAnimationFrame(detectionLoop);
  } catch (error) {
    state.cameraOn = false;
    setBox(cameraStatus, `Camera did not start: ${error.name || "Error"} ${error.message}`, "bad");
  }
});

document.querySelector("#camera-stop").addEventListener("click", () => {
  state.cameraOn = false;
  state.lastFrameMs = null;
  state.tracks = [];
  state.zoneHit = null;
  if (state.stream) {
    for (const track of state.stream.getTracks()) {
      track.stop();
    }
  }
  video.srcObject = null;
  setBox(cameraStatus, "Camera is off.", "warn");
  detectStatus.textContent = "Camera is off. No detection is not an all-clear.";
});

document.querySelector("#model-load").addEventListener("click", async () => {
  state.modelError = false;
  setBox(modelStatus, "Downloading TensorFlow.js and COCO-SSD onto this phone.", "warn");
  try {
    if (!window.tf) {
      await loadScript(TFJS_URL);
    }
    if (!window.cocoSsd) {
      await loadScript(COCO_URL);
    }
    state.model = await window.cocoSsd.load({ base: MODEL_BASE });
    state.modelReady = true;
    setBox(
      modelStatus,
      "Model loaded on this phone. Weights came from storage.googleapis.com. The picture is not uploaded.",
      "ok"
    );
  } catch (error) {
    state.modelReady = false;
    state.modelError = true;
    setBox(modelStatus, `Model did not load: ${error.message}`, "bad");
  }
});

function manualEnabled(enabled) {
  manualHazard.disabled = !enabled;
  manualUnavailable.disabled = !enabled;
  manualRoute.disabled = !enabled;
}

manualMode.addEventListener("change", () => {
  if (manualMode.checked) {
    detectEnable.checked = false;
  }
  manualEnabled(manualMode.checked);
});

detectEnable.addEventListener("change", () => {
  if (detectEnable.checked) {
    manualMode.checked = false;
    manualHazard.checked = false;
    manualUnavailable.checked = false;
    manualRoute.value = "0";
  }
  manualEnabled(false);
});

async function connectGatt(device) {
  const server = await device.gatt.connect();
  const service = await server.getPrimaryService(SERVICE_UUID);
  const command = await service.getCharacteristic(COMMAND_UUID);
  const status = await service.getCharacteristic(STATUS_UUID);
  state.server = server;
  state.commandChar = command;
  state.queue = createWriteQueue(async (bytes) => {
    if (command.properties.writeWithoutResponse) {
      await command.writeValueWithoutResponse(bytes);
    } else {
      await command.writeValue(bytes);
    }
  });
  if (status.properties.notify) {
    await status.startNotifications();
    status.addEventListener("characteristicvaluechanged", (event) => {
      const decoded = decodeStatus(new Uint8Array(event.target.value.buffer));
      boardStatus.textContent = decoded
        ? `Board ${decoded.linkState === 1 ? "link outage" : "link heartbeat"}, sequence ${decoded.sequence}, pins hazard ${decoded.hazardOn} unavailable ${decoded.unavailableOn} route ${decoded.routeOn} link ${decoded.linkOn}.`
        : "Board status did not match the 8-byte status packet.";
    });
  }
  setBox(bleStatus, `Connected to ${device.name || "the board"}.`, "ok");
}

async function reconnect() {
  if (!state.device || state.reconnectsLeft <= 0) {
    setBox(bleStatus, "Bluetooth disconnected. Tap Connect to try again.", "bad");
    state.queue = null;
    state.commandChar = null;
    return;
  }
  state.reconnectsLeft -= 1;
  setBox(bleStatus, "Bluetooth dropped. Trying to reconnect.", "warn");
  try {
    await connectGatt(state.device);
    state.reconnectsLeft = 3;
  } catch (error) {
    setTimeout(reconnect, 1000);
  }
}

document.querySelector("#ble-connect").addEventListener("click", async () => {
  if (!navigator.bluetooth) {
    setBox(bleStatus, "This page cannot see Web Bluetooth.", "bad");
    return;
  }
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
    });
    state.device = device;
    state.reconnectsLeft = 3;
    device.addEventListener("gattserverdisconnected", () => {
      state.queue = null;
      state.commandChar = null;
      reconnect();
    });
    await connectGatt(device);
  } catch (error) {
    setBox(bleStatus, `Bluetooth did not connect: ${error.message}`, "bad");
  }
});

document.querySelector("#rotation-button").addEventListener("click", async () => {
  try {
    if (typeof DeviceOrientationEvent !== "undefined" && typeof DeviceOrientationEvent.requestPermission === "function") {
      const answer = await DeviceOrientationEvent.requestPermission();
      if (answer !== "granted") {
        orientationText.textContent = "Rotation permission was not granted. Rotation is not position.";
        return;
      }
    }
    window.addEventListener("deviceorientation", (event) => {
      orientationText.textContent = `Rotation only: alpha ${event.alpha}, beta ${event.beta}, gamma ${event.gamma}. This is not 6DoF and not a distance.`;
    }, { once: false });
  } catch (error) {
    orientationText.textContent = `Rotation is unavailable: ${error.message}. It is not used for a warning.`;
  }
});

document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "hidden") {
    state.zoneHit = null;
    detectStatus.textContent = "This page is in the background. The image warning is cleared. That is not an all-clear.";
  }
});

setInterval(sendClaim, HEARTBEAT_MS);
manualEnabled(false);
diagnosticsText();
