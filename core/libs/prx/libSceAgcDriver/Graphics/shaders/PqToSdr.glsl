float pqToneMap(float luminance, float relativeWhite) {
    const float knee = 0.75;
    if (luminance <= knee) return luminance;
    float t = (luminance - knee) / (1.0 - knee);
    float w = (relativeWhite - knee) / (1.0 - knee);
    return knee + (1.0 - knee) * t * (1.0 + t / (w * w)) / (1.0 + t);
}

vec3 pqToSdrLinear(vec3 nits, float whiteNits) {
    float referenceWhite = min(203.0, whiteNits);
    vec3 bt2020 = nits / referenceWhite;
    vec3 rgb = max(vec3(
        1.6604910 * bt2020.r - 0.5876411 * bt2020.g - 0.0728499 * bt2020.b,
        -0.1245505 * bt2020.r + 1.1328999 * bt2020.g - 0.0083494 * bt2020.b,
        -0.0181508 * bt2020.r - 0.1005789 * bt2020.g + 1.1187297 * bt2020.b), vec3(0.0));
    float luminance = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
    if (luminance > 0.0) rgb = min(rgb * (pqToneMap(luminance, whiteNits / referenceWhite) / luminance), vec3(1.0));
    return rgb;
}

float srgbEncode(float linear) {
    float v = clamp(linear, 0.0, 1.0);
    return v <= 0.0031308 ? 12.92 * v : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
}

vec3 srgbEncode(vec3 linear) {
    return vec3(srgbEncode(linear.r), srgbEncode(linear.g), srgbEncode(linear.b));
}
