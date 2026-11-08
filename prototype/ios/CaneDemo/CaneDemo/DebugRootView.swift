import ARKit
import SceneKit
import SwiftUI

struct DebugRootView: View {
    @ObservedObject var demo: DemoController

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text("Supervised cane demo")
                    .font(.largeTitle)
                    .accessibilityAddTraits(.isHeader)
                Text("Not a mobility aid. The existing cane stays in use. This native build does not command the left or right buzzer. Quiet is not an all-clear.")
                    .font(.body)
                CameraPreview(session: demo.session)
                    .frame(height: 220)
                    .accessibilityLabel("Rear camera preview")
                sourceSection
                cameraSection
                claimSection
                if demo.sourceIsSimulation {
                    simulationSection
                }
                linkSection
            }
            .padding()
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .onAppear { demo.prepare() }
    }

    private var sourceSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Outgoing source")
                .font(.title2)
            Picker("Outgoing source", selection: Binding(
                get: { demo.sourceIsSimulation },
                set: { demo.setSource(simulation: $0) }
            )) {
                Text("Camera pipeline").tag(false)
                Text("Simulation").tag(true)
            }
            .pickerStyle(.segmented)
            if demo.sourceIsSimulation {
                Text("SIMULATED INPUT. These claims are not from the camera.")
                    .font(.headline)
                    .accessibilityAddTraits(.isHeader)
            }
        }
    }

    private var cameraSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Camera readout")
                .font(.title2)
            labeled("World tracking supported", demo.worldTrackingSupported ? "Yes" : "No")
            labeled("sceneDepth supported", demo.sceneDepthSupported ? "Yes, but this build does not enable it" : "No")
            labeled("Camera permission", demo.cameraAuthorized ? "Authorized" : "Not authorized")
            labeled("Tracking", demo.trackingText)
            labeled("Device motion", demo.displacementText)
            Text("The step above is ARKit camera travel between consecutive normal frames. It is not an obstacle distance and it is not a speed.")
                .font(.footnote)
            labeled("Frame interval", demo.frameIntervalText)
            labeled("Plane context", demo.planeText)
            Text("A plane hit is the distance from the ARKit camera pose to an existing detected plane. It is not a clearance and it does not raise a hazard.")
                .font(.footnote)
        }
    }

    private var claimSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Outgoing claim")
                .font(.title2)
            Text(demo.claimText)
                .font(.title3)
            Text(demo.sendText)
        }
    }

    private var simulationSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Left and right buzzers")
                .font(.title2)
            Text("This native screen does not sound the cane buzzers. Use the Bluefy page for left and right image feedback. A camera packet from this app keeps both side bits off.")
        }
    }

    private var linkSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Controller link")
                .font(.title2)
            labeled("Phone", demo.bluetoothText)
            labeled("Board status", demo.boardText)
            Text("The board, not the phone, decides link outage. Until a status notification arrives, board output is unknown. The phone sends a packet about ten times a second while connected. That interval is the send timer, not a measured end-to-end latency.")
                .font(.footnote)
        }
    }

    private func labeled(_ title: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title)
                .font(.headline)
            Text(value)
                .font(.body)
        }
        .accessibilityElement(children: .combine)
    }
}

struct CameraPreview: UIViewRepresentable {
    let session: ARSession

    func makeUIView(context: Context) -> ARSCNView {
        let view = ARSCNView(frame: .zero)
        view.session = session
        view.automaticallyUpdatesLighting = false
        view.scene = SCNScene()
        return view
    }

    func updateUIView(_ uiView: ARSCNView, context: Context) {
        if uiView.session !== session {
            uiView.session = session
        }
    }
}
