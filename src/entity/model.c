#include "model.h"
#include "model_defs.h"

const ModelDef *model_def(int m) { return &MODEL_DEFS[m]; }

// corners (BL, BR, TR, TL seen from outside) per face, same convention as the chunk mesher.
static const u8 FC[6][4][3] = {
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},  // -X  (model's right side)
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},  // +X  (left side)
    {{1, 0, 1}, {0, 0, 1}, {0, 0, 0}, {1, 0, 0}},  // -Y  bottom
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},  // +Y  top
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},  // -Z  back
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},  // +Z  front
};
static const f32 SHADE[6] = {0.8f, 0.8f, 0.55f, 1.0f, 0.7f, 0.9f};

static u32 shade_col(u32 c, f32 s) {
    return rgba_u32((int)((c & 255) * s), (int)(((c >> 8) & 255) * s), (int)(((c >> 16) & 255) * s), (int)(c >> 24));
}

static void draw_box(const ModelPartDef *p, mat4 m, u32 color, f32 tw, f32 th) {
    f32 w = p->size[0], h = p->size[1], d = p->size[2];
    f32 iw = roundf(w), ih = roundf(h), id = roundf(d);
    if (iw < 1) iw = 1;
    if (ih < 1) ih = 1;
    if (id < 1) id = 1;
    f32 u = (f32)p->u, v = (f32)p->v;
    // texture rectangles (x, y, w, h) per face, Minecraft box-UV layout
    f32 R[6][4] = {
        {u, v + id, id, ih},                    // -X right side
        {u + id + iw, v + id, id, ih},          // +X left side
        {u + id + iw, v, iw, id},               // bottom
        {u + id, v, iw, id},                    // top
        {u + 2 * id + iw, v + id, iw, ih},      // back
        {u + id, v + id, iw, ih},               // front
    };
    for (int f = 0; f < 6; f++) {
        vec3 pts[4]; f32 uv[8];
        static const f32 CUV[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
        for (int i = 0; i < 4; i++) {
            vec3 lp = v3((p->box[0] + FC[f][i][0] * w) / 16.0f, (p->box[1] + FC[f][i][1] * h) / 16.0f, (p->box[2] + FC[f][i][2] * d) / 16.0f);
            pts[i] = m4_mul_point(m, lp);
            uv[i * 2] = (R[f][0] + CUV[i][0] * R[f][2]) / tw;
            uv[i * 2 + 1] = (R[f][1] + CUV[i][1] * R[f][3]) / th;
        }
        b3d_quad_uv(pts, uv, shade_col(color, SHADE[f]));
    }
}

static mat4 part_matrix(const ModelPartDef *p, mat4 world, const ModelPose *ps) {
    f32 rx = DEG2RAD(p->rot[0]), ry = DEG2RAD(p->rot[1]), rz = DEG2RAD(p->rot[2]);
    f32 sw = ps ? ps->limb_swing * 0.6662f : 0, amt = ps ? ps->limb_amount : 0;
    if (ps) switch (p->role) {
    case PR_HEAD: rx += -ps->head_pitch; ry += ps->head_yaw; break;
    case PR_LEG_R: case PR_LEG_FR: case PR_LEG_BL: rx += cosf(sw) * 1.4f * amt; break;
    case PR_LEG_L: case PR_LEG_FL: case PR_LEG_BR: rx += cosf(sw + PI_F) * 1.4f * amt; break;
    case PR_ARM_R: case PR_ARM_L: {
        f32 side = p->role == PR_ARM_R ? 1.0f : -1.0f;
        if (ps->arms_forward) {
            rx += -PI_F / 2 + sinf(ps->age * 0.067f) * 0.05f * side;
            rx += -sinf(ps->attack * PI_F) * 0.6f;
        } else if (ps->bow_aim > 0) {
            rx += -PI_F / 2 * ps->bow_aim; ry += side * 0.1f;
        } else {
            rx += cosf(sw + (p->role == PR_ARM_R ? PI_F : 0)) * 1.0f * amt;
            if (p->role == PR_ARM_R) rx += -sinf(ps->attack * PI_F) * 1.2f;
        }
        rz += side * (cosf(ps->age * 0.09f) * 0.05f + 0.05f);
    } break;
    case PR_WING_R: case PR_WING_L: {
        f32 flap = ps->airborne ? (sinf(ps->age * 1.3f) * 0.5f + 0.6f) : 0;
        rz += (p->role == PR_WING_R ? 1 : -1) * flap;
    } break;
    case PR_SPIDER_LEG: {
        f32 ph = (p->idx / 2) * (PI_F / 2);
        f32 s = p->pivot[0] > 0 ? 1.0f : -1.0f;
        ry += sinf(sw * 2 + ph) * 0.4f * amt * s;
        rz += fabsf(cosf(sw * 2 + ph)) * 0.35f * amt * s;
    } break;
    case PR_TAIL: {
        f32 sp = ps->swimming ? 0.35f : 0.12f;
        ry += sinf(ps->age * sp + p->idx) * (ps->swimming ? 0.5f : 0.25f) * (1.0f + amt);
    } break;
    case PR_FIN: rz += sinf(ps->age * 0.3f) * 0.3f; break;
    case PR_TENTACLE: rx += sinf(ps->age * 0.12f + p->idx * 0.8f) * 0.35f + 0.15f; break;
    case PR_ROD: {   // blaze rods / breeze swirls orbit the body
        f32 a = ps->age * 0.06f * (p->idx % 2 ? 1.0f : -1.0f);
        f32 px = p->pivot[0], pz = p->pivot[2];
        f32 c = cosf(a), s = sinf(a);
        mat4 m = m4_mul(world, m4_translate(v3((px * c - pz * s) / 16.0f, (p->pivot[1] + sinf(ps->age * 0.1f + p->idx) * 1.5f) / 16.0f, (px * s + pz * c) / 16.0f)));
        return m;
    }
    case PR_JAW: rx += ps->jaw * 0.6f; break;
    default: break;
    }
    mat4 m = m4_mul(world, m4_translate(v3(p->pivot[0] / 16.0f, p->pivot[1] / 16.0f, p->pivot[2] / 16.0f)));
    m = m4_mul(m, m4_rot_y(ry));
    m = m4_mul(m, m4_rot_z(rz));
    m = m4_mul(m, m4_rot_x(rx));
    return m;
}

void model_draw(int model, mat4 world, const ModelPose *pose, u32 color, f32 tw, f32 th) {
    const ModelDef *md = &MODEL_DEFS[model];
    for (int i = 0; i < md->n; i++) draw_box(&md->parts[i], part_matrix(&md->parts[i], world, pose), color, tw, th);
}

void model_draw_part(int model, int idx, mat4 world, u32 color, f32 tw, f32 th) {
    const ModelDef *md = &MODEL_DEFS[model];
    if (idx < 0 || idx >= md->n) return;
    draw_box(&md->parts[idx], world, color, tw, th);
}
