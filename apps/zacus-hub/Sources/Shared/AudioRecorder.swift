import Foundation
import AVFoundation

@MainActor
final class AudioRecorder: NSObject, ObservableObject, AVAudioRecorderDelegate {
    enum State: Equatable {
        case idle
        case requestingPermission
        case recording(startedAt: Date)
        case stopping
        case denied
        case failed(String)
    }

    @Published private(set) var state: State = .idle
    @Published private(set) var lastClipURL: URL?

    private var recorder: AVAudioRecorder?

    func start() async {
        guard case .idle = state else { return }
        state = .requestingPermission
        let granted = await Self.requestPermission()
        guard granted else {
            state = .denied
            return
        }
        do {
            try Self.configureSession()
            let url = Self.makeClipURL()
            let settings: [String: Any] = [
                AVFormatIDKey: kAudioFormatMPEG4AAC,
                AVSampleRateKey: 16_000.0,
                AVNumberOfChannelsKey: 1,
                AVEncoderAudioQualityKey: AVAudioQuality.medium.rawValue
            ]
            let rec = try AVAudioRecorder(url: url, settings: settings)
            rec.delegate = self
            guard rec.record() else {
                state = .failed("AVAudioRecorder.record() returned false")
                return
            }
            recorder = rec
            lastClipURL = url
            state = .recording(startedAt: Date())
        } catch {
            state = .failed(error.localizedDescription)
        }
    }

    /// Stops the recorder and returns the captured clip URL when ready.
    func stop() async -> URL? {
        guard case .recording = state, let rec = recorder else {
            state = .idle
            return nil
        }
        state = .stopping
        rec.stop()
        // give the encoder a tick to flush
        try? await Task.sleep(nanoseconds: 120_000_000)
        let url = rec.url
        recorder = nil
        state = .idle
        Self.deactivateSession()
        return url
    }

    func reset() {
        recorder?.stop()
        recorder = nil
        state = .idle
        Self.deactivateSession()
    }

    // MARK: - helpers

    private static func makeClipURL() -> URL {
        let dir = FileManager.default.temporaryDirectory
        let name = "ptt-\(Int(Date().timeIntervalSince1970*1000)).m4a"
        return dir.appendingPathComponent(name)
    }

    private static func requestPermission() async -> Bool {
        #if os(iOS)
        if #available(iOS 17, *) {
            return await AVAudioApplication.requestRecordPermission()
        } else {
            return await withCheckedContinuation { cont in
                AVAudioSession.sharedInstance().requestRecordPermission { cont.resume(returning: $0) }
            }
        }
        #else
        return await withCheckedContinuation { cont in
            AVCaptureDevice.requestAccess(for: .audio) { cont.resume(returning: $0) }
        }
        #endif
    }

    private static func configureSession() throws {
        #if os(iOS)
        let session = AVAudioSession.sharedInstance()
        try session.setCategory(.playAndRecord, mode: .voiceChat, options: [.defaultToSpeaker, .allowBluetooth])
        try session.setActive(true, options: .notifyOthersOnDeactivation)
        #endif
    }

    private static func deactivateSession() {
        #if os(iOS)
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
        #endif
    }
}
