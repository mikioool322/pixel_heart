from pathlib import Path

source_path = Path(__file__).resolve().parent.parent / 'src' / 'animations' / 'FireworksAnimation.cpp'
source = source_path.read_text()

assert 'case \'T\':' in source
assert 'case 0u: return 0b111u;' in source
assert 'case 1u: return 0b010u;' in source
assert 'case 2u: return 0b010u;' in source
assert 'case 3u: return 0b010u;' in source
assert 'case 4u: return 0b010u;' in source
assert "drawBirthdayGlyph(matrix, 'O', 2u, 3u" in source
assert "drawBirthdayGlyph(matrix, 'T', 7u, 3u" in source
assert "drawBirthdayGlyph(matrix, 'S', 12u, 3u" in source
assert "drawBirthdayGlyph(matrix, 'T', 2u, 9u" in source
assert "drawBirthdayGlyph(matrix, 'A', 7u, 9u" in source
assert "drawBirthdayGlyph(matrix, 'L', 12u, 9u" in source
assert 'const uint8_t bit = static_cast<uint8_t>(1u << (width - 1u - col));' in source
assert 'const uint8_t mirroredCol = static_cast<uint8_t>(width - 1u - col);' in source
print('birthday layout checks ok')
