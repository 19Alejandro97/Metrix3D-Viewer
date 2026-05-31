#include "datum_entity.hpp"
#include "math_utils.hpp"

DatumEntity DatumEntity::FromCircle(const Vec3& p1, const Vec3& p2, const Vec3& p3, const std::string& objId) {
    DatumEntity e;
    e.type = Type::Circle;
    e.objectId = objId;
    e.pickedPoints = {p1, p2, p3};
    e.isValid = MathUtils::Calculate3PointCircle(p1, p2, p3, e.point, e.radius, e.normal);
    return e;
}

DatumEntity DatumEntity::FromPlane(const Vec3& p1, const Vec3& p2, const Vec3& p3, const std::string& objId) {
    DatumEntity e;
    e.type = Type::Plane;
    e.objectId = objId;
    e.pickedPoints = {p1, p2, p3};

    Vec3 v1 = p2 - p1;
    Vec3 v2 = p3 - p1;
    Vec3 n = glm::cross(v1, v2);
    double len = glm::length(n);
    if (len < 1e-6) {
        e.isValid = false;
        return e;
    }
    e.normal = n / len;
    e.point = (p1 + p2 + p3) / 3.0; // centroid
    e.isValid = true;
    return e;
}

DatumEntity DatumEntity::FromLine(const Vec3& p1, const Vec3& p2, const std::string& objId) {
    DatumEntity e;
    e.type = Type::Line;
    e.objectId = objId;
    e.pickedPoints = {p1, p2};
    e.point = p1;
    e.extra = p2;
    Vec3 dir = p2 - p1;
    double len = glm::length(dir);
    if (len < 1e-6) { e.isValid = false; return e; }
    e.normal = dir / len; // direction stored in normal field for lines
    e.radius = len;       // length stored in radius for lines
    e.isValid = true;
    return e;
}
