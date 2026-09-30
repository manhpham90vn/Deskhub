import Foundation

struct PairedDeviceRow: Identifiable {
    let name: String
    let shortKey: String
    let fingerprint: String

    var id: String { fingerprint }
}
