#ifndef AIRCRAFT_STATE_H
#define AIRCRAFT_STATE_H

#include <raymath.h>

#define EARTH_RADIUS 6371000.0

struct aircraft_state {
    double lat, lon, alt;
    float speed; // m/s
    Vector3 forward, up, right;
};

void aircraft_update(struct aircraft_state *a, float dt) {
    float d = a->speed * dt; // meters traveled

    float east = a->forward.x;
    float up = a->forward.y;
    float north = a->forward.z;

    float dist_east = east * d;
    float dist_north = north * d;
    float dist_up = up * d;

    // Meters per degree (local linearization)
    double metersPerDegLat = 111320.0; // TODO figure out this magic num
    double metersPerDegLon = 111320.0 * cos(a->lat * DEG2RAD);

    // convert motion to degrees
    double dLat = dist_north / metersPerDegLat;
    double dLon = dist_east / metersPerDegLon;

    // update geodetic position
    a->lat += dLat;
    a->lon += dLon;
    a->alt += dist_up;
}

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
