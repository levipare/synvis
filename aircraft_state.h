#ifndef AIRCRAFT_STATE_H
#define AIRCRAFT_STATE_H

#include <raymath.h>

#define EARTH_RADIUS 6371000.0

struct aircraft_state {
    Vector3 pos; // lat, alt, lon
    Vector3 forward, up, right;
    float speed;
};

void aircraft_update(struct aircraft_state *a, float dt) {
    float d = a->speed * dt; // meters traveled

    // Decompose forward vector into ENU components.
    // Assuming a->forward = {East, Up, North}
    float east = a->forward.x;
    float up = a->forward.y;
    float north = a->forward.z;

    // Distance traveled in each horizontal axis
    float dEast = east * d;
    float dNorth = north * d;
    float dUp = up * d;

    // Meters per degree (local linearization)
    const double deg2rad = M_PI / 180.0;
    double metersPerDegLat = 111320.0;
    double metersPerDegLon = 111320.0 * cos(a->pos.x * deg2rad);

    // Convert motion to degrees
    double dLat = dNorth / metersPerDegLat;
    double dLon = dEast / metersPerDegLon;

    // Update geodetic position
    a->pos.x += dLat;
    a->pos.z += dLon;
    a->pos.y += dUp;
}

// void aircraft_update(struct aircraft_state *a, float dt) {
//     Vector3 delta = Vector3Scale(a->forward, dt * a->speed);
//     a->pos = Vector3Add(a->pos, delta);
// }

void aircraft_pitch(struct aircraft_state *a, float rad) {
    a->forward = Vector3Normalize(Vector3RotateByAxisAngle(a->forward, a->right, rad));
    a->up = Vector3Normalize(Vector3RotateByAxisAngle(a->up, a->right, rad));
}

void aircraft_roll(struct aircraft_state *a, float rad) {
    a->right = Vector3Normalize(Vector3RotateByAxisAngle(a->right, a->forward, rad));
    a->up = Vector3Normalize(Vector3RotateByAxisAngle(a->up, a->forward, rad));
}

void aircraft_yaw(struct aircraft_state *a, float rad) {
    a->forward = Vector3Normalize(Vector3RotateByAxisAngle(a->forward, a->up, rad));
    a->right = Vector3Normalize(Vector3RotateByAxisAngle(a->right, a->up, rad));
}

#endif
