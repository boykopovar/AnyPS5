#version 450
layout(early_fragment_tests) in;
layout(binding = 0, std430) buffer Results { uint passed; };
void main() {
    atomicAdd(passed, 1);
}
