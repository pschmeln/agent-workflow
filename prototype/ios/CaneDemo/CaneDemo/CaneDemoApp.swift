import SwiftUI

@main
struct CaneDemoApp: App {
    @StateObject private var demo = DemoController()

    var body: some Scene {
        WindowGroup {
            DebugRootView(demo: demo)
        }
    }
}
