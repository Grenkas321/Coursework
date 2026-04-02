class Scip < Formula
  desc "Solver for mixed integer programming and mixed integer nonlinear programming"
  homepage "https://scipopt.org"
  url "https://scipopt.org/download/release/scip-10.0.0.tgz"
  sha256 "b91d2ed32c422a13c502c37cf296eb9550e55d6bd311e61bfa6dfb9811b03d87"
  license "Apache-2.0"

  livecheck do
    url "https://github.com/scipopt/scip"
    strategy :github_latest
  end

  depends_on "cmake" => :build
  depends_on "boost" => :no_linkage
  depends_on "cppad" => :no_linkage
  depends_on "gmp"
  depends_on "ipopt"
  depends_on "mpfr" => :no_linkage
  depends_on "openblas"
  depends_on "papilo"
  depends_on "readline"
  depends_on "soplex"
  depends_on "tbb"

  uses_from_macos "zlib"

  on_macos do
    depends_on "gcc"
  end

  def install
    system "cmake", "-S", ".", "-B", "build", "-DZIMPL=OFF", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"

    pkgshare.install "check/instances/MIP/enigma.mps"
    pkgshare.install "check/instances/MINLP/gastrans.nl"
    pkgshare.install "check/instances/MIPEX/flugpl_rational.mps"
  end

  test do
    expected = "problem is solved [optimal solution found]"
    assert_match expected, shell_output("#{bin}/scip -f #{pkgshare}/enigma.mps")
    assert_match expected, shell_output("#{bin}/scip -f #{pkgshare}/gastrans.nl")

    command = "set exact enable TRUE read #{pkgshare}/flugpl_rational.mps optimize quit"
    assert_match expected, shell_output("#{bin}/scip -c \"#{command}\"")
  end
end
