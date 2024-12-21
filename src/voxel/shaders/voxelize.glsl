#include <daxa/daxa.inl>

#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(MyPushConstant, push)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
void main() {
}
