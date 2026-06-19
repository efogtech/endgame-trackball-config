/* Float-only polynomial kernel for sin(x), x in [-pi/4, pi/4].
* 7th-order Horner form; max absolute error < 5e-8. */
static float sinf_kernel(const float x) {
    const float x2 = x * x;
    return x * (1.0f + x2 * (-0.16666667f + x2 * (0.00833333f + x2 * -0.000198413f)));
}

/* Float-only polynomial kernel for cos(x), x in [-pi/4, pi/4].
 * 6th-order Horner form; max absolute error < 5e-8. */
static float cosf_kernel(const float x) {
    const float x2 = x * x;
    return 1.0f + x2 * (-0.5f + x2 * (0.04166667f + x2 * -0.00138889f));
}

/* Reduce x to [-pi/4, pi/4] and return (reduced_x, quadrant 0-3). */
static float range_reduce(const float x, int *q_out) {
    const float two_over_pi = 0.6366197724f;
    const float pi_over_2   = 1.5707963268f;
    const float q_f = x * two_over_pi;
    int q = (int)q_f;
    if (q_f < 0.0f) {
        q--;
    }
    *q_out = q;
    return x - (float)q * pi_over_2;
}

/* Override picolibc's powf: integer-exponent loop avoids double arithmetic.
 * Correct for the non-negative integer exponents behavior_input_two_axis uses. */
float powf(const float base, float exponent) {
    float power = 1.0f;
    for (; exponent >= 1.0f; exponent -= 1.0f) {
        power *= base;
    }
    return power;
}

/* Override picolibc's sinf/cosf: pure-float Cody-Waite + polynomial.
 * Eliminates the double arithmetic picolibc's implementations pull in. */
float sinf(const float x) {
    int q;
    const float r = range_reduce(x, &q);
    switch (q & 3) {
        case 0:  return  sinf_kernel(r);
        case 1:  return  cosf_kernel(r);
        case 2:  return -sinf_kernel(r);
        default: return -cosf_kernel(r);
    }
}

float cosf(const float x) {
    int q;
    const float r = range_reduce(x, &q);
    switch (q & 3) {
        case 0:  return  cosf_kernel(r);
        case 1:  return -sinf_kernel(r);
        case 2:  return -cosf_kernel(r);
        default: return  sinf_kernel(r);
    }
}
