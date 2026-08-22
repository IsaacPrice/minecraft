#pragma once

#include <glm/glm.hpp>

// The six planes of the view frustum, pulled straight out of a view-projection
// matrix, and a box test against them.
//
// Every loaded chunk used to be drawn every frame. At a render distance of 64
// that is 4225 chunks and 8450 draw calls, for a camera that can only ever see
// the ninety degrees or so in front of it -- so roughly three quarters of that
// work was submitted, transformed, and clipped away.
//
// The planes come out of the matrix by the standard trick: a point is inside
// the left plane when its clip space x is greater than -w, which rearranges to
// (row3 + row0) . p >= 0, and so on around the other five.
struct Frustum
{
    glm::vec4 planes[6];

    explicit Frustum(const glm::mat4& viewProjection)
    {
        const glm::mat4& m = viewProjection;

        // glm is column major, so m[column][row]. Reading a row means taking
        // the same row index out of each of the four columns.
        glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
        glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
        glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
        glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);

        planes[0] = row3 + row0; // left
        planes[1] = row3 - row0; // right
        planes[2] = row3 + row1; // bottom
        planes[3] = row3 - row1; // top
        planes[4] = row3 + row2; // near
        planes[5] = row3 - row2; // far

        for (int i = 0; i < 6; i++)
        {
            float length = glm::length(glm::vec3(planes[i]));
            if (length > 0.0f)
                planes[i] /= length;
        }
    }

    // Conservative: returns true for a box that is outside the frustum but
    // inside the slab of one of its planes, which costs a stray draw call and
    // never drops a chunk that should have been drawn.
    bool Intersects(const glm::vec3& low, const glm::vec3& high) const
    {
        for (int i = 0; i < 6; i++)
        {
            const glm::vec4& plane = planes[i];

            // The corner of the box furthest along the plane normal. If even
            // that one is behind the plane, the whole box is.
            glm::vec3 furthest(plane.x >= 0.0f ? high.x : low.x,
                               plane.y >= 0.0f ? high.y : low.y,
                               plane.z >= 0.0f ? high.z : low.z);

            if (glm::dot(glm::vec3(plane), furthest) + plane.w < 0.0f)
                return false;
        }

        return true;
    }
};
