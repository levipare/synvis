#ifndef AIRCRAFT_STATE_H
#define AIRCRAFT_STATE_H

#include <cglm/struct/vec3.h>
#include <math.h>

#include <cglm/struct.h>

#define WGS84_A 6378.137       // semi-major axis (km)
#define WGS84_B 6356.752314245 // semi-minor axis (km)
#define WGS84_E2 ((WGS84_A * WGS84_A - WGS84_B * WGS84_B) / (WGS84_A * WGS84_A)) // eccentricity^2

vec3s geodetic_to_ecef(double lat, double lon, double h) {
    double sinLat = sin(lat);
    double cosLat = cos(lat);
    double sinLon = sin(lon);
    double cosLon = cos(lon);

    double N = WGS84_A / sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    vec3s ecef;
    ecef.x = (N + h) * cosLat * cosLon;
    ecef.y = (N + h) * cosLat * sinLon;
    ecef.z = ((1.0 - WGS84_E2) * N + h) * sinLat;

    return ecef;
}

struct aircraft_state {
    double lat, lon, height;
    float max_speed, throttle;
    vec3s pos, forward, up, right;
};

void aircraft_update(struct aircraft_state *ac, float dt) {
    float d = ac->throttle * ac->max_speed * dt / 1000; // meters traveled
    ac->pos = glms_vec3_add(ac->pos, glms_vec3_scale(ac->forward, d));
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
