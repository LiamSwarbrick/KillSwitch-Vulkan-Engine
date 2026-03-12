#define VERTEX_TYPE_STATIC 0
#define VERTEX_TYPE_SKINNED 1

#define BLEND_MODE_OPAQUE   0
#define BLEND_MODE_MASKED   1
#define BLEND_MODE_BLEND    2
#define BLEND_MODE_ADDITIVE 3

#ifndef __cplusplus
layout(constant_id = 0) const uint CURRENT_VERTEX_TYPE = 0;
layout(constant_id = 1) const uint CURRENT_BLEND_MODE = 0;
#endif
