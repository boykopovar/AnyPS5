#version 450
layout(location = 0) flat in vec4 color;
layout(location = 0) out vec4 outputColor;
void main() {
    outputColor = color;
}
