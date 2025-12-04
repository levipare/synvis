#ifndef AIRCRAFT_STATE_H
#define AIRCRAFT_STATE_H

#include <cglm/struct/vec3.h>
#include <math.h>

#include <cglm/struct.h>

#define WGS84_A 6378137.0         // major axis
#define WGS84_E2 6.69437999014e-3 // eccentricity^2

vec3s geodetic_to_ecef(double lat, double lon, double alt) {
    double sinLat = sin(lat);
    double cosLat = cos(lat);
    double sinLon = sin(lon);
    double cosLon = cos(lon);

    double N = WGS84_A / sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    vec3s ecef;
    ecef.x = (N + alt) * cosLat * cosLon;
    ecef.y = (N + alt) * cosLat * sinLon;
    ecef.z = ((1.0 - WGS84_E2) * N + alt) * sinLat;

    return ecef;
}

vec3s ecef_to_enu(vec3s p, vec3s origin, double lat0, double lon0) {
    double sinLat = sin(lat0), cosLat = cos(lat0);
    double sinLon = sin(lon0), cosLon = cos(lon0);

    // Translate to origin
    double dx = p.x - origin.x;
    double dy = p.y - origin.y;
    double dz = p.z - origin.z;

    vec3s enu;

    // ENU conversion matrix
    enu.x = -sinLon * dx + cosLon * dy;                                 // east
    enu.y = -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz; // north
    enu.z = cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz;  // up

    return enu;
}

vec3s geodetic_to_enu(double lat, double lon, double alt, double lat0, double lon0,
                      vec3s originECEF) {

    vec3s pECEF = geodetic_to_ecef(lat, lon, alt);
    return ecef_to_enu(pECEF, originECEF, lat0, lon0);
}

struct aircraft_state {
    double lat, lon, height;
    float throttle;
    vec3s forward, up, right;
};

#define SPEED 1000
void aircraft_update(struct aircraft_state *a, float dt) {
    float d = a->throttle * SPEED * dt; // meters traveled

    vec3s dist = glms_vec3_scale(a->forward, d);

    // Meters per degree (local linearization)
    double metersPerDegLat = 111320.0; // TODO figure out this magic num
    double metersPerDegLon = 111320.0 * cos(glm_rad(a->lat));

    // convert motion to degrees
    double dLat = dist.z / metersPerDegLat;
    double dLon = dist.x / metersPerDegLon;

    // update geodetic position
    a->lat += dLat;
    a->lon += dLon;
    a->height += dist.y;
}

void aircraft_pitch(struct aircraft_state *a, float rad) {
    a->forward = glms_vec3_normalize(glms_vec3_rotate(a->forward, rad, a->right));
    a->up = glms_vec3_normalize(glms_vec3_rotate(a->up, rad, a->right));
}

void aircraft_roll(struct aircraft_state *a, float rad) {
    a->right = glms_vec3_normalize(glms_vec3_rotate(a->right, rad, a->forward));
    a->up = glms_vec3_normalize(glms_vec3_rotate(a->up, rad, a->forward));
}

void aircraft_yaw(struct aircraft_state *a, float rad) {
    a->forward = glms_vec3_normalize(glms_vec3_rotate(a->forward, rad, a->up));
    a->right = glms_vec3_normalize(glms_vec3_rotate(a->right, rad, a->up));
}

#endif
