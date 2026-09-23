class DeskhubCli < Formula
  desc "Command-line client for Deskhub, the LAN remote desktop"
  homepage "https://github.com/manhpham90vn/Deskhub"
  url "https://github.com/manhpham90vn/Deskhub/releases/download/v@VERSION@/deskhub-cli-v@VERSION@-macos"
  version "@VERSION@"
  sha256 "@SHA256@"
  license "MIT"

  depends_on :macos

  def install
    bin.install "deskhub-cli-v#{version}-macos" => "deskhub-cli"
    chmod 0555, bin/"deskhub-cli"
  end

  test do
    system bin/"deskhub-cli", "version"
  end
end
