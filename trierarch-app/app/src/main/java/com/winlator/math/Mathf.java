package com.winlator.math;

import android.util.Rational;

public abstract class Mathf {
    public static final float EPSILON = 1e-5f;

    public static float clamp(float x, float min, float max) {
        return (x < min) ? min : ((x > max) ? max : x);
    }

    public static int clamp(int x, int min, int max) {
        return (x < min) ? min : (x > max ? max : x);
    }

    public static float roundTo(float x, float step) {
        return roundTo(x, step, true);
    }

    public static float roundTo(float x, float step, boolean roundHalfDown) {
        return (float)((roundHalfDown ? Math.floor(x / step) : Math.round(x / step)) * step);
    }

    public static int roundPoint(float x) {
        return (int)(x <= 0 ? Math.floor(x) : Math.ceil(x));
    }

    public static byte sign(float x) {
        return (byte)(x < 0 ? -1 : (x > 0 ? 1 : 0));
    }

    public static float lengthSq(float x, float y) {
        return x * x + y * y;
    }

    public static float distance(float x0, float y0, float x1, float y1) {
        return (float)Math.hypot(x0 - x1, y0 - y1);
    }

    public static float fract(float x) {
        return x - (float)Math.floor(x);
    }

    public static int floorToInt(float x) {
        return (int)Math.floor(x);
    }

    public static int ceilToInt(float x) {
        return (int)Math.ceil(x);
    }

    public static int roundToInt(float x) {
        return Math.round(x);
    }

    public static boolean isPowerOfTwo(int x) {
        return (x & (x - 1)) == 0;
    }

    public static int nextPowerOfTwo(int x) {
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x + 1;
    }

    public static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    public static float inverseLerp(float a, float b, float x) {
        return (x - a) / (b - a);
    }

    public static float angleLerp(float a, float b, float t) {
        float delta = (float)((b - a) % (Math.PI * 2));
        if (delta > Math.PI) delta -= Math.PI * 2;
        if (delta < -Math.PI) delta += Math.PI * 2;
        return a + delta * t;
    }

    public static float modulo(float x, float mod) {
        return (x % mod + mod) % mod;
    }

    public static Rational toRational(float x, int maxDenominator) {
        int sign = x < 0 ? -1 : 1;
        x = Math.abs(x);
        int a = (int)Math.floor(x);
        int numerator0 = 1;
        int denominator0 = 0;
        int numerator1 = a;
        int denominator1 = 1;
        float frac = x - a;
        while (Math.abs(x - (float)numerator1 / denominator1) > EPSILON && denominator1 < maxDenominator && frac > EPSILON) {
            frac = 1 / frac;
            a = (int)Math.floor(frac);
            int numerator2 = a * numerator1 + numerator0;
            int denominator2 = a * denominator1 + denominator0;
            numerator0 = numerator1;
            denominator0 = denominator1;
            numerator1 = numerator2;
            denominator1 = denominator2;
            frac -= a;
        }
        return new Rational(sign * numerator1, denominator1);
    }
}

