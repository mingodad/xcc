#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH        (256)
#define HEIGHT       (256)
#define NSUBSAMPLES  (2)
#define NAO_SAMPLES  (8)

typedef struct {double x, y, z;} vec;

static double vdot(const vec *v0, const vec *v1) {
  return v0->x * v1->x + v0->y * v1->y + v0->z * v1->z;
}

static void vnormalize(vec *c) {
  double length = sqrt(vdot(c, c));
  if (length > 1.0e-17) {
    c->x /= length;
    c->y /= length;
    c->z /= length;
  }
}

double EPS = 1.0e-6;

typedef struct {
  double t;
  vec p;
  vec n;
} Isect;

typedef struct {
  vec center;
  double radius;
} Sphere;

typedef struct {
  vec p;
  vec n;
} Plane;

typedef struct {
  vec org;
  vec dir;
} Ray;

void ray_sphere_intersect(Isect *isect, const Ray *ray, const Sphere *sphere) {
  vec rs;
  rs.x = ray->org.x - sphere->center.x;
  rs.y = ray->org.y - sphere->center.y;
  rs.z = ray->org.z - sphere->center.z;

  double B = vdot(&rs, &ray->dir);
  double C = vdot(&rs, &rs) - sphere->radius * sphere->radius;
  double D = B * B - C;
  if (D > 0.0) {
    double t = -B - sqrt(D);
    if (t > EPS && t < isect->t) {
      isect->t = t;

      isect->p.x = ray->org.x + ray->dir.x * t;
      isect->p.y = ray->org.y + ray->dir.y * t;
      isect->p.z = ray->org.z + ray->dir.z * t;

      isect->n.x = isect->p.x - sphere->center.x;
      isect->n.y = isect->p.y - sphere->center.y;
      isect->n.z = isect->p.z - sphere->center.z;
      vnormalize(&isect->n);
    }
  }
}

void ray_plane_intersect(Isect *isect, const Ray *ray, const Plane *plane) {
  double d = -vdot(&plane->p, &plane->n);
  double v = vdot(&ray->dir, &plane->n);

  if (fabs(v) < EPS)
    return;

  double t = -(vdot(&ray->org, &plane->n) + d) / v;
  if (t > EPS && t < isect->t) {
    isect->t = t;

    isect->p.x = ray->org.x + ray->dir.x * t;
    isect->p.y = ray->org.y + ray->dir.y * t;
    isect->p.z = ray->org.z + ray->dir.z * t;

    isect->n = plane->n;
  }
}

double copysign(double x, double f) {
  return f >= 0 ? x : -x;
}

void orthoBasis(vec *basis, const vec *n) {
  basis[2] = *n;
  double sign = copysign(1.0, n->z);
  const double a = -1.0 / (sign + n->z);
  const double b = n->x * n->y * a;
  basis[0].x = 1.0 + sign * n->x * n->x * a;
  basis[0].y = sign * b;
  basis[0].z = -sign * n->x;
  basis[1].x = b;
  basis[1].y = sign + n->y * n->y * a;
  basis[1].z = -n->y;
}

const Sphere spheres[3] = {
  {{-2.0, 0, -3.5},  0.5},
  {{-0.5, 0, -3.0},  0.5},
  {{ 1.0, 0, -2.2},  0.5},
};

const Plane plane = {
  {0.0, -0.5, 0.0},
  {0.0,  1.0, 0.0},
};

vec ambient_occlusion(const Isect *isect) {
  int ntheta = NAO_SAMPLES;
  int nphi   = NAO_SAMPLES;

  vec basis[3];
  orthoBasis(basis, &isect->n);

  int occlusion = 0;
  for (int j = 0; j < ntheta; ++j) {
    for (int i = 0; i < nphi; ++i) {
      double theta = sqrt(drand48());
      double phi   = 2.0 * M_PI * drand48();

      double x = cos(phi) * theta;
      double y = sin(phi) * theta;
      double z = sqrt(1.0 - theta * theta);

      // local -> global
      double rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;
      double ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;
      double rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;

      Ray ray;
      ray.org = isect->p;
      ray.dir.x = rx;
      ray.dir.y = ry;
      ray.dir.z = rz;

      Isect occIsect;
      occIsect.t   = HUGE_VAL;

      ray_sphere_intersect(&occIsect, &ray, &spheres[0]);
      ray_sphere_intersect(&occIsect, &ray, &spheres[1]);
      ray_sphere_intersect(&occIsect, &ray, &spheres[2]);
      ray_plane_intersect (&occIsect, &ray, &plane);

      if (occIsect.t < HUGE_VAL)
        ++occlusion;
    }
  }

  double c = (ntheta * nphi - occlusion) / (double)(ntheta * nphi);
  return (vec){c, c, c};
}

unsigned char clamp(double f) {
  int i = (int)(f * 255.5);
  if (i < 0) i = 0;
  else if (i > 255) i = 255;
  return i;
}

void render(unsigned char *img, int w, int h, int nsubsamples) {
  double coeff = 1.0 / (nsubsamples * nsubsamples);
  unsigned char *dst = img;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      double cr = 0, cg = 0, cb = 0;
      for (int v = 0; v < nsubsamples; ++v) {
        for (int u = 0; u < nsubsamples; ++u) {
          double px =  (x + (u / (double)nsubsamples) - (w / 2.0)) / (w / 2.0);
          double py = -(y + (v / (double)nsubsamples) - (h / 2.0)) / (h / 2.0);

          Ray ray;
          ray.org.x = 0.0;
          ray.org.y = 0.0;
          ray.org.z = 0.0;

          ray.dir.x = px;
          ray.dir.y = py;
          ray.dir.z = -1.0;
          vnormalize(&ray.dir);

          Isect isect;
          isect.t = HUGE_VAL;

          ray_sphere_intersect(&isect, &ray, &spheres[0]);
          ray_sphere_intersect(&isect, &ray, &spheres[1]);
          ray_sphere_intersect(&isect, &ray, &spheres[2]);
          ray_plane_intersect (&isect, &ray, &plane);

          if (isect.t < HUGE_VAL) {
            vec col = ambient_occlusion(&isect);
            cr += col.x;
            cg += col.y;
            cb += col.z;
          }
        }
      }

      *dst++ = clamp(cr * coeff);
      *dst++ = clamp(cg * coeff);
      *dst++ = clamp(cb * coeff);
      *dst++ = 255;
    }
  }
}

extern void showGraphic(int width, int height, void *img);

int main(void) {
  unsigned char *img = malloc(WIDTH * HEIGHT * 4);
  render(img, WIDTH, HEIGHT, NSUBSAMPLES);
  showGraphic(WIDTH, HEIGHT, img);
  return 0;
}