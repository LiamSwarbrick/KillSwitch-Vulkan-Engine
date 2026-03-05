import math

sigma = 9.0
radius = 21  # (43x43 kernel)


weights = [math.exp(-(x*x) / (2*sigma*sigma)) for x in range(radius + 1)]

# Normalize (accounting for symmetry)
total = weights[0] + 2 * sum(weights[1:])
weights = [w / total for w in weights]

# Linear sampling (we can half our taps by sampling inbetween texels thus having 22 taps per pass instead of 43)
# TODO

pairs = []
for i in range(1, radius, 2):  # 1, 3, 4, ..., 19
    w0 = weights[i]
    w1 = weights[i + 1]
    w = w0 + w1
    offset = (i*w0 + (i+1)*w1) / w
    pairs.append((offset, w))

# Shader code:
print("// Weights generated from ./bloom_print_weights.py")
print("// 43-wide Gaussian blur, linear-sampling optimised")
print("// 1 center tap + 10 paired taps per side = 43 taps total")
print("const float CENTER_WEIGHT = " + str(weights[0]) + ";")
print("const int PAIRED_WEIGHT_COUNT = " + str(len(pairs)) + ";")
print("const float offsets[PAIRED_WEIGHT_COUNT] = float[](")

for i in range(len(pairs)):
    print("    " + str(pairs[i][0]), end="")
    if i != len(pairs) - 1:
        print(",")
    else:
        print()

print(");")
print("const float weights[PAIRED_WEIGHT_COUNT] = float[](")

for i in range(len(pairs)):
    print("    " + str(pairs[i][1]), end="")
    if i != len(pairs) - 1:
        print(",")
    else:
        print()

print(");")

