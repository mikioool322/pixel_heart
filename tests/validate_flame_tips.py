from pathlib import Path

project_root = Path(__file__).resolve().parent.parent
source_path = project_root / 'src' / 'animations' / 'FlameAnimation.cpp'
header_path = project_root / 'include' / 'animations' / 'FlameAnimation.h'
source = source_path.read_text()
header = header_path.read_text()

assert 'static uint8_t rowHeatScale(uint8_t y);' in header
assert 'static uint8_t rowRenderThreshold(uint8_t y);' in header
assert 'static Color tipColor(uint8_t heat);' in header
assert 'static bool isUpperThird(uint8_t y);' in header
assert 'uint8_t columnTopY[kWidth];' in source
assert 'uint8_t columnTopHeat[kWidth];' in source
assert 'if (topY > 0u && (isUpperThird(topY) || topY <= 7u) && localPeak && topHeat >= 110u)' in source
assert 'const bool extend = ((frameCounter_ + x + topHeat) % 3u) != 0u;' in source
assert 'matrix.setPixel(x, topY - 1u, tipColor(extendedHeat));' in source
assert 'if (topY <= 5u && localValley && topHeat < 135u)' in source
assert 'matrix.setPixel(x, topY, Colors::Black);' in source
assert 'tipColor(topHeat)' in source
assert 'Colors::FlameWhite' in source
print('flame tip shaping checks ok')
