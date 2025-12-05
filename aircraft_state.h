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

void ecef_to_geodetic(vec3s ecef, double *lat, double *lon, double *h) {
    double a = WGS84_A;
    double e2 = WGS84_E2;

    float X = ecef.x;
    float Y = ecef.y;
    float Z = ecef.z;

    *lon = atan2(Y, X);

    double r = sqrt(X * X + Y * Y);

    // Bowring's formula initial approximation
    double E2 = a * a - (a * a * (1.0 - e2));
    double F = 54.0 * (a * a * (1.0 - e2)) * Z * Z;
    double G = r * r + (1.0 - e2) * Z * Z - e2 * E2;
    double c = (e2 * e2 * F * r * r) / (G * G * G);
    double s = cbrt(1.0 + c + sqrt(c * c + 2 * c));
    double P = F / (3.0 * (s + 1.0 / s + 1.0) * (s + 1.0 / s + 1.0) * G * G);
    double Q = sqrt(1.0 + 2.0 * e2 * e2 * P);
    double r0 = -(P * e2 * r) / (1.0 + Q) +
                sqrt(0.5 * a * a * (1.0 + 1.0 / Q) - P * (1.0 - e2) * Z * Z / (Q * (1.0 + Q)) -
                     0.5 * P * r * r);
    double U = sqrt((r - e2 * r0) * (r - e2 * r0) + Z * Z);
    double V = sqrt((r - e2 * r0) * (r - e2 * r0) + (1.0 - e2) * Z * Z);
    double z0 = (a * a * (1.0 - e2) * Z) / (a * V);

    *lat = atan2(Z + z0 * e2, r);

    double sinLat = sin(*lat);
    double N = a / sqrt(1.0 - e2 * sinLat * sinLat);

    *h = r / cos(*lat) - N;
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
