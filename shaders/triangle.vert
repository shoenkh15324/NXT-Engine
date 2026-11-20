#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform ObjectPushConstants {
    mat4 transform;
    vec4 color;
}
object;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = object.transform * vec4(inPosition, 1.0);
    fragColor = object.color.rgb;
}
