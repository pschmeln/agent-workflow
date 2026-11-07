import ARKit
import AVFoundation
import CoreBluetooth
import Foundation
import simd
import UIKit

final class DemoController: NSObject, ObservableObject, ARSessionDelegate, CBCentralManagerDelegate, CBPeripheralDelegate {
    let session = ARSession()

    @Published var sourceIsSimulation = false
    @Published var simulatedHazard = false
    @Published var simulatedUnavailable = false
    @Published var simulatedRoute = CANE_ROUTE_NONE

    @Published var cameraAuthorized = false
    @Published var worldTrackingSupported = false
    @Published var sceneDepthSupported = false
    @Published var trackingText = "Not started"
    @Published var displacementText = "Unknown"
    @Published var frameIntervalText = "Unknown"
    @Published var planeText = "No existing-plane hit"
    @Published var claimText = "No packet yet"
    @Published var sendText = "Not sending"
    @Published var bluetoothText = "Bluetooth starting"
    @Published var boardText = "No controller status received"

    private var hasPlaneHit = false
    private var planeHitMeters: Float = 0
    private var tracking = CANE_TRACKING_UNSUPPORTED
    private var previousNormalPosition: SIMD3<Float>?
    private var previousTimestamp: TimeInterval?
    private var sequence: UInt32 = 0
    private var latestPacket: Data?
    private var timer: Timer?

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var commandCharacteristic: CBCharacteristic?
    private var statusCharacteristic: CBCharacteristic?

    private let serviceUUID = CBUUID(string: "8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1001")
    private let commandUUID = CBUUID(string: "8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1002")
    private let statusUUID = CBUUID(string: "8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1003")

    override init() {
        super.init()
        session.delegate = self
        central = CBCentralManager(delegate: self, queue: nil)
        timer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
            self?.sendLatest()
        }
    }

    func prepare() {
        worldTrackingSupported = ARWorldTrackingConfiguration.isSupported
        sceneDepthSupported = ARWorldTrackingConfiguration.supportsFrameSemantics(.sceneDepth)
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized:
            cameraAuthorized = true
            startSession()
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .video) { [weak self] granted in
                DispatchQueue.main.async {
                    self?.cameraAuthorized = granted
                    if granted {
                        self?.startSession()
                    } else {
                        self?.refreshClaim()
                    }
                }
            }
        default:
            cameraAuthorized = false
            refreshClaim()
        }
    }

    func startSession() {
        guard worldTrackingSupported else {
            tracking = CANE_TRACKING_UNSUPPORTED
            trackingText = "World tracking is not supported on this device"
            refreshClaim()
            return
        }
        let configuration = ARWorldTrackingConfiguration()
        configuration.worldAlignment = .gravity
        configuration.planeDetection = [.horizontal, .vertical]
        configuration.frameSemantics = []
        session.run(configuration, options: [.resetTracking, .removeExistingAnchors])
        trackingText = "AR session started. sceneDepth is not enabled."
        refreshClaim()
    }

    func setSource(simulation: Bool) {
        sourceIsSimulation = simulation
        if !simulation {
            simulatedHazard = false
            simulatedUnavailable = false
            simulatedRoute = CANE_ROUTE_NONE
        }
        refreshClaim()
    }

    func refreshClaim() {
        let input = currentInput()
        var mutable = input
        var assessment = cane_assessment_t()
        cane_assess(&mutable, &assessment)
        var command = cane_command_t()
        sequence &+= 1
        let stamp = UInt16(truncatingIfNeeded: UInt64(Date().timeIntervalSince1970 * 1000))
        cane_command_from_assessment(&assessment, sequence, stamp, &command)
        var packet = [UInt8](repeating: 0, count: Int(CANE_PACKET_BYTES))
        if cane_command_encode(&command, &packet) == 1 {
            latestPacket = Data(packet)
            claimText = describe(assessment)
            sendText = "Packet \(sequence) ready"
        } else {
            latestPacket = nil
            claimText = describe(assessment)
            sendText = "Shared checker refused the packet"
        }
    }

    private func currentInput() -> cane_input_t {
        var input = cane_input_t(
            source: sourceIsSimulation ? CANE_SOURCE_SIMULATION : CANE_SOURCE_CAMERA,
            tracking: tracking,
            camera_authorized: Int32(cameraAuthorized ? 1 : 0),
            world_tracking_supported: Int32(worldTrackingSupported ? 1 : 0),
            scene_depth_supported: Int32(sceneDepthSupported ? 1 : 0),
            has_plane_hit: 0,
            plane_hit_meters: 0,
            simulation_elevated_hazard: 0,
            simulation_unavailable: 0,
            route_cue: CANE_ROUTE_NONE
        )
        if sourceIsSimulation {
            input.simulation_elevated_hazard = Int32(simulatedHazard ? 1 : 0)
            input.simulation_unavailable = Int32(simulatedUnavailable ? 1 : 0)
            input.route_cue = simulatedRoute
        } else {
            input.has_plane_hit = Int32(hasPlaneHit ? 1 : 0)
            input.plane_hit_meters = planeHitMeters
        }
        return input
    }

    private func describe(_ assessment: cane_assessment_t) -> String {
        let source = assessment.source == CANE_SOURCE_SIMULATION ? "Simulation" : "Camera pipeline"
        let hazard = assessment.hazard == CANE_HAZARD_ELEVATED ? "Elevated hazard claim" : "No hazard claim"
        let availability = assessment.system_unavailable == 1 ? "System unavailable" : "Pipeline running"
        let route: String
        switch assessment.route_cue {
        case CANE_ROUTE_DEMO_LEFT:
            route = "Demo route cue: left label"
        case CANE_ROUTE_DEMO_RIGHT:
            route = "Demo route cue: right label"
        default:
            route = "No route cue"
        }
        return "\(source). \(hazard). \(availability). \(route). This is not an all-clear."
    }

    private func sendLatest() {
        guard let latestPacket, let peripheral, let commandCharacteristic else {
            return
        }
        guard peripheral.state == .connected else {
            return
        }
        let writeType: CBCharacteristicWriteType = commandCharacteristic.properties.contains(.writeWithoutResponse)
            ? .withoutResponse
            : .withResponse
        peripheral.writeValue(latestPacket, for: commandCharacteristic, type: writeType)
        sendText = "Last write attempted for sequence \(sequence)"
    }

    func session(_ session: ARSession, didUpdate frame: ARFrame) {
        let state = frame.camera.trackingState
        let code: Int32
        let label: String
        switch state {
        case .normal:
            code = CANE_TRACKING_NORMAL
            label = "Tracking normal"
        case .limited(let reason):
            code = CANE_TRACKING_LIMITED
            label = "Tracking limited (\(reasonText(reason)))"
        case .notAvailable:
            code = CANE_TRACKING_UNAVAILABLE
            label = "Tracking not available"
        }
        let position = SIMD3<Float>(
            frame.camera.transform.columns.3.x,
            frame.camera.transform.columns.3.y,
            frame.camera.transform.columns.3.z
        )
        var displacement: String = "Unknown"
        if code == CANE_TRACKING_NORMAL, let previous = previousNormalPosition {
            var meters: Float = 0
            let ok = cane_camera_displacement_m(
                previous.x, previous.y, previous.z,
                position.x, position.y, position.z,
                1,
                &meters
            )
            if ok == 1 {
                displacement = String(format: "ARKit camera step %.3f m since the previous normal frame", meters)
            }
        }
        if code == CANE_TRACKING_NORMAL {
            previousNormalPosition = position
        } else {
            previousNormalPosition = nil
        }
        var interval = "Unknown"
        if let previousTimestamp {
            let delta = frame.timestamp - previousTimestamp
            if delta > 0 {
                interval = String(format: "ARKit frame interval %.3f s", delta)
            }
        }
        previousTimestamp = frame.timestamp

        var planeValid = false
        var planeMeters: Float = 0
        var planeLabel = "No existing-plane hit"
        if let query = frame.raycastQuery(from: CGPoint(x: 0.5, y: 0.5), allowing: .existingPlaneGeometry, alignment: .any) {
            if let hit = session.raycast(query).first {
                let hitPosition = SIMD3<Float>(
                    hit.worldTransform.columns.3.x,
                    hit.worldTransform.columns.3.y,
                    hit.worldTransform.columns.3.z
                )
                let step = simd_distance(position, hitPosition)
                planeValid = true
                planeMeters = step
                planeLabel = String(format: "Existing-plane hit %.2f m from the camera pose. Not obstacle clearance.", step)
            }
        }

        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.tracking = code
            self.trackingText = label
            self.displacementText = displacement
            self.frameIntervalText = interval
            self.hasPlaneHit = planeValid
            self.planeHitMeters = planeMeters
            self.planeText = planeLabel
            self.refreshClaim()
        }
    }

    func session(_ session: ARSession, didFailWithError error: Error) {
        DispatchQueue.main.async { [weak self] in
            self?.tracking = CANE_TRACKING_UNAVAILABLE
            self?.trackingText = "AR session failed: \(error.localizedDescription)"
            self?.previousNormalPosition = nil
            self?.refreshClaim()
        }
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            bluetoothText = "Scanning for CaneDemo"
            central.scanForPeripherals(withServices: [serviceUUID], options: nil)
        case .poweredOff:
            bluetoothText = "Bluetooth is off"
        case .unauthorized:
            bluetoothText = "Bluetooth permission denied"
        case .unsupported:
            bluetoothText = "Bluetooth LE is not supported"
        default:
            bluetoothText = "Bluetooth state \(central.state.rawValue)"
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        self.peripheral = peripheral
        peripheral.delegate = self
        central.stopScan()
        bluetoothText = "Connecting to \(peripheral.name ?? "cane controller")"
        central.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        bluetoothText = "Connected. Discovering service."
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        commandCharacteristic = nil
        statusCharacteristic = nil
        boardText = "Controller disconnected. The board watchdog should enter link outage."
        bluetoothText = "Disconnected. Scanning again."
        central.scanForPeripherals(withServices: [serviceUUID], options: nil)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error {
            bluetoothText = "Service discovery failed: \(error.localizedDescription)"
            return
        }
        guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
            bluetoothText = "Cane service missing"
            return
        }
        peripheral.discoverCharacteristics([commandUUID, statusUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error {
            bluetoothText = "Characteristic discovery failed: \(error.localizedDescription)"
            return
        }
        commandCharacteristic = service.characteristics?.first(where: { $0.uuid == commandUUID })
        statusCharacteristic = service.characteristics?.first(where: { $0.uuid == statusUUID })
        if let statusCharacteristic {
            peripheral.setNotifyValue(true, for: statusCharacteristic)
        }
        bluetoothText = commandCharacteristic == nil ? "Command characteristic missing" : "Link ready for packets"
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, characteristic.uuid == statusUUID, let value = characteristic.value else {
            return
        }
        let bytes = [UInt8](value)
        var status = cane_status_t()
        let decoded = bytes.withUnsafeBufferPointer { buffer -> Int32 in
            guard let base = buffer.baseAddress else { return 0 }
            return cane_status_decode(base, buffer.count, &status)
        }
        guard decoded == 1 else {
            boardText = "Controller status failed the shared decoder"
            return
        }
        let link = status.link_state == CANE_LINK_OUTAGE ? "link outage" : "link heartbeat"
        boardText = "\(link). sequence \(status.last_sequence). pins hazard \(status.hazard_on) unavailable \(status.unavailable_on) route \(status.route_on) link \(status.link_on)"
    }

    private func reasonText(_ reason: ARCamera.TrackingState.Reason) -> String {
        switch reason {
        case .initializing:
            return "initializing"
        case .excessiveMotion:
            return "excessive motion"
        case .insufficientFeatures:
            return "insufficient features"
        case .relocalizing:
            return "relocalizing"
        @unknown default:
            return "unspecified"
        }
    }
}
