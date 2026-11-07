export const SERVICE_UUID = "8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1001";
export const COMMAND_UUID = "8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1002";
export const STATUS_UUID = "8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1003";

export const PACKET_VERSION = 1;
export const PACKET_BYTES = 18;
export const PLANE_CM_NONE = 0xffff;
export const WATCHDOG_MS = 500;
export const HEARTBEAT_MS = 200;
export const STALE_MS = 750;

export const SOURCE_CAMERA = 1;
export const SOURCE_SIMULATION = 2;
export const SOURCE_IMAGE_HEURISTIC = 3;

export const HAZARD_NO_CLAIM = 0;
export const HAZARD_ELEVATED = 1;

export const ROUTE_NONE = 0;
export const ROUTE_DEMO_LEFT = 1;
export const ROUTE_DEMO_RIGHT = 2;

export const TRACKING_UNSUPPORTED = 0;
export const TRACKING_UNAVAILABLE = 1;
export const TRACKING_NORMAL = 3;

export function crc16(bytes) {
  let crc = 0xffff;
  for (let i = 0; i < bytes.length; i += 1) {
    crc ^= bytes[i] << 8;
    for (let bit = 0; bit < 8; bit += 1) {
      if ((crc & 0x8000) !== 0) {
        crc = ((crc << 1) ^ 0x1021) & 0xffff;
      } else {
        crc = (crc << 1) & 0xffff;
      }
    }
  }
  return crc;
}

export function commandAdmissible(command) {
  if (
    command.source !== SOURCE_CAMERA &&
    command.source !== SOURCE_SIMULATION &&
    command.source !== SOURCE_IMAGE_HEURISTIC
  ) {
    return false;
  }
  if (command.hazard !== HAZARD_NO_CLAIM && command.hazard !== HAZARD_ELEVATED) {
    return false;
  }
  if (command.unavailable !== 0 && command.unavailable !== 1) {
    return false;
  }
  if (
    command.route !== ROUTE_NONE &&
    command.route !== ROUTE_DEMO_LEFT &&
    command.route !== ROUTE_DEMO_RIGHT
  ) {
    return false;
  }
  if (command.tracking < TRACKING_UNSUPPORTED || command.tracking > TRACKING_NORMAL) {
    return false;
  }
  if (command.source === SOURCE_CAMERA) {
    if (command.hazard !== HAZARD_NO_CLAIM || command.route !== ROUTE_NONE) {
      return false;
    }
  }
  if (command.source === SOURCE_IMAGE_HEURISTIC) {
    if (command.route !== ROUTE_NONE || command.planeCm !== PLANE_CM_NONE) {
      return false;
    }
  }
  return true;
}

function write16(bytes, offset, value) {
  bytes[offset] = value & 0xff;
  bytes[offset + 1] = (value >> 8) & 0xff;
}

function write32(bytes, offset, value) {
  bytes[offset] = value & 0xff;
  bytes[offset + 1] = (value >> 8) & 0xff;
  bytes[offset + 2] = (value >> 16) & 0xff;
  bytes[offset + 3] = (value >> 24) & 0xff;
}

export function encodeCommand(command) {
  if (!commandAdmissible(command)) {
    return null;
  }
  const bytes = new Uint8Array(PACKET_BYTES);
  bytes[0] = PACKET_VERSION;
  bytes[1] = command.source;
  bytes[2] = command.hazard;
  bytes[3] = command.unavailable;
  bytes[4] = command.route;
  bytes[5] = command.tracking;
  bytes[6] = command.sceneDepth ? 1 : 0;
  bytes[7] = 0;
  write32(bytes, 8, command.sequence >>> 0);
  write16(bytes, 12, command.timeMsLow & 0xffff);
  write16(bytes, 14, command.planeCm & 0xffff);
  const crc = crc16(bytes.subarray(0, 16));
  write16(bytes, 16, crc);
  return bytes;
}

export function decodeStatus(bytes) {
  if (bytes.length !== 8 || bytes[0] !== PACKET_VERSION || bytes[3] !== 0) {
    return null;
  }
  if (bytes[1] !== 0 && bytes[1] !== 1) {
    return null;
  }
  return {
    linkState: bytes[1],
    hazardOn: (bytes[2] & 1) !== 0,
    unavailableOn: (bytes[2] & 2) !== 0,
    routeOn: (bytes[2] & 4) !== 0,
    linkOn: (bytes[2] & 8) !== 0,
    sequence: (bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24)) >>> 0,
  };
}

export function createWriteQueue(writeFn) {
  let chain = Promise.resolve();
  return {
    enqueue(bytes) {
      const run = chain.then(() => writeFn(bytes));
      chain = run.then(
        () => undefined,
        () => undefined
      );
      return run;
    },
  };
}
