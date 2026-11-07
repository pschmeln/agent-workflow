export const CENTER_MARGIN = 0.25;
export const IOU_THRESHOLD = 0.3;
export const TRACK_MAX_AGE_MS = 500;
export const SUPPORTED_CLASSES = ["person", "bicycle", "car", "motorcycle", "bus", "truck"];
export const MIN_SCORE = 0.5;

export function iou(a, b) {
  const ax2 = a.x + a.w;
  const ay2 = a.y + a.h;
  const bx2 = b.x + b.w;
  const by2 = b.y + b.h;
  const ix = Math.max(0, Math.min(ax2, bx2) - Math.max(a.x, b.x));
  const iy = Math.max(0, Math.min(ay2, by2) - Math.max(a.y, b.y));
  const inter = ix * iy;
  const union = a.w * a.h + b.w * b.h - inter;
  if (union <= 0) {
    return 0;
  }
  return inter / union;
}

export function updateTracks(previous, detections, nowMs) {
  const used = new Set();
  const next = [];
  let nextId = previous.reduce((max, track) => Math.max(max, track.id), 0);
  const pool = previous.filter((track) => nowMs - track.lastSeen <= TRACK_MAX_AGE_MS);
  for (const detection of detections) {
    let best = null;
    let bestScore = IOU_THRESHOLD;
    for (const track of pool) {
      if (used.has(track.id)) {
        continue;
      }
      const score = iou(track.bbox, detection.bbox);
      if (score >= bestScore) {
        best = track;
        bestScore = score;
      }
    }
    if (best) {
      used.add(best.id);
      next.push({
        id: best.id,
        className: detection.className,
        score: detection.score,
        bbox: detection.bbox,
        lastSeen: nowMs,
      });
    } else {
      nextId += 1;
      next.push({
        id: nextId,
        className: detection.className,
        score: detection.score,
        bbox: detection.bbox,
        lastSeen: nowMs,
      });
    }
  }
  return next;
}

export function centerZoneHit(tracks, width, height, nowMs) {
  if (width <= 0 || height <= 0) {
    return null;
  }
  const x0 = width * CENTER_MARGIN;
  const x1 = width * (1 - CENTER_MARGIN);
  const y0 = height * CENTER_MARGIN;
  const y1 = height * (1 - CENTER_MARGIN);
  let best = null;
  for (const track of tracks) {
    if (nowMs - track.lastSeen > TRACK_MAX_AGE_MS) {
      continue;
    }
    if (!SUPPORTED_CLASSES.includes(track.className) || track.score < MIN_SCORE) {
      continue;
    }
    const cx = track.bbox.x + track.bbox.w / 2;
    const cy = track.bbox.y + track.bbox.h / 2;
    if (cx < x0 || cx > x1 || cy < y0 || cy > y1) {
      continue;
    }
    if (!best || track.score > best.score) {
      best = {
        trackId: track.id,
        className: track.className,
        score: track.score,
        zone: "center",
      };
    }
  }
  return best;
}
