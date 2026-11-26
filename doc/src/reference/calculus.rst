.. meta::
   :description: Find derivatives, integrals, and limits in SpeedCrunch.

Calculus
========

.. function:: diff(expression)
              diff(expression; variable)
              diff(expression; order)
              diff(expression; variable; order)

    Computes the symbolic derivative of an expression with respect to a
    variable, optionally to a specified order.

    With no variable given, SpeedCrunch uses the only variable in the
    expression. A supported expression with no variables gives zero.
    If there is more than one variable, give the name of the one to use.
    SpeedCrunch treats the other names as constants. Built-in constants
    such as π and e do not count as variables. Variable names in the
    expression keep their symbolic meaning even if they have saved values.
    In the expression, ``ans`` uses the last answer if it is a unitless number.

    The order defaults to 1. Use a non-negative whole number without units.
    Order 0 returns the original expression without inferring a variable.
    In a two-argument call, a single variable name selects the variable.
    The order can use arithmetic or elementary functions. To use a saved
    number as the order, write ``n+0`` or use the three-argument form.

    Examples::

        diff(x^4)           = 4*x^3
        diff(x^4; 2)        = 12*x^2
        diff(x*y; x)        = y
        diff(x^2*y^3; y; 2)  = 6*x^2*y
        diff(x*y; 0)        = x*y

    ``diff()`` supports arithmetic, powers, square and cube roots,
    exponential and logarithmic functions, trigonometric and
    hyperbolic functions and their inverses, and the error functions.

    SpeedCrunch simplifies the result. Coefficients stay exact up to 120
    digits, and longer ones give a limit error, as in ``diff(x^100; 70)``.
    Like terms combine, polynomials are multiplied out or factored,
    fractions go over a common denominator, and common factors cancel,
    also polynomial factors with exact numeric coefficients. A new form is
    only used when it is shorter, so of two forms with the same length the
    first one stays, as in ``6*x^2-3``. The simplification has some limits:

    - It does not use trigonometric identities such as
      ``sin(x)^2+cos(x)^2 = 1``.
    - It does not combine products of exponentials, as in
      ``exp(x^2)*exp(-x^2)``.
    - It does not take common factors out of a sum that contains fractions.
    - Cancelling a common factor can remove a point where the expression
      is undefined, as in ``diff(x^2/x) = 1``. A square root never becomes
      something that is defined in more places, so its domain stays.

    In the result display, square roots show as ``√``, and the power of a
    root stays outside, as in ``√(1+x²)¹¹``.

    Trigonometric functions use the active angle mode. For example,
    ``diff(sin(x))`` gives ``cos(x)`` in radians and ``π/180*cos(x)``
    in degrees. Inverse trigonometric derivatives use the reciprocal
    angle factor. Use the same angle mode when evaluating a copied formula.

    Linear units can be constant factors, as in ``diff(x[m])``. Unit
    conversions and units with an offset give an error. Use bare angles
    in trigonometric arguments that depend on the variable.
    A symbolic result can be copied back into the editor. Numeric
    operations need a numeric result.

    Unsupported expressions and calculations that exceed the size,
    nesting, or work limits give an error.

.. function:: integrate(expression)
              integrate(expression; variable)
              integrate(expression; lower; upper)
              integrate(expression; variable; lower; upper)

    Computes the indefinite or definite integral of an expression with
    respect to a variable.

    With no variable given, SpeedCrunch uses the only variable in the
    expression, or ``x`` if there are none. Give the variable name if the
    expression has more than one. The other names act as constants.
    Built-in constants such as π and e do not count as variables.
    As with :func:`diff`, existing variable names stay symbolic and
    ``ans`` can supply a unitless number.

    Without bounds, the result is one antiderivative. The general solution
    is that expression plus any constant. SpeedCrunch supports powers,
    polynomials, simple rational functions, and some combinations of
    exponential, logarithmic, trigonometric, and hyperbolic functions.
    This includes some forms that need substitution or integration by parts.
    A polynomial times the exponential of a quadratic, such as
    ``x^2*exp(-x^2)``, gives the error function :func:`erf` if the
    quadratic opens downwards, or :func:`erfi` if it opens upwards.
    A polynomial times an error function is also supported.
    If there is no supported rule, the calculation gives an error.

    These rules have some limits:

    - The leading coefficient of the quadratic must be a number, so
      ``exp(-a*x^2)`` with a variable ``a`` is not supported. An exponent
      with units only works with numerical integration over finite bounds.
    - A polynomial can have a degree up to 24, or up to 23 next to an
      error function.
    - Other products with error functions, such as ``x*exp(-x^2)*erf(x)``,
      and exponentials that cancel, such as ``exp(x^2)*exp(-x^2)``, are not
      supported.
    - Antiderivatives are not simplified like the results of :func:`diff`,
      so equal antiderivatives can look different.

    With bounds, SpeedCrunch first tries to use an exact antiderivative.
    Bounds can be expressions. A bound can contain a free variable if
    SpeedCrunch can find an exact result and check the domain. Reversing the
    bounds changes the sign. Equal bounds give zero when the integrand is
    defined there.

    If a definite integral has no supported antiderivative, SpeedCrunch
    can try adaptive numerical integration with real bounds and a real
    integrand. The ``≈`` sign marks an approximation. SpeedCrunch estimates
    the error using the calculator's precision and gives an error if the
    calculation cannot reach its target.

    Some improper integrals are supported, such as ``1/sqrt(x)`` or
    ``ln(x)`` at zero, and a decaying exponential or a Gaussian over an
    infinite interval. Over an infinite interval, the error function
    goes to 1 or -1, which gives exact results such as ``√π``. An
    integrand that grows or falls no faster than ``1/x`` towards infinity,
    such as ``exp(x^2)`` or ``x/(x^2+1)``, gives an error.
    Type ``inf`` or ``-inf`` for an infinite bound.
    With automatic input adjustments on, the result display shows these
    bounds with ``∞``.
    Divergent integrals give an error even when opposite sides of a pole
    would cancel. SpeedCrunch cannot decide every integral over an infinite
    interval: ``erf(x)-1`` and ``exp(-x^2)*erfi(x)`` from 0 to ``inf`` give
    an error, although the first converges and the second diverges.

    An exact result such as ``π/4`` or ``√π`` cannot be used in further
    arithmetic, as in ``2*integrate(exp(-x^2); -inf; inf)``. Numeric
    operations need a numeric result.

    Trigonometric functions follow the active angle mode. Use bare angles
    in the integrand and bounds. For physical quantities, both bounds must
    have compatible dimensions. The result has the integrand's dimensions
    multiplied by the integration variable's dimensions.

    Examples in radians::

        integrate(x^2)                = x^3/3
        integrate(x*y; y)             = x*y^2/2
        integrate(1/(1+x^2); 0; 1)    = π/4
        integrate(3[m/s]; 0[s]; 2[s])  = 6[m]
        integrate(exp(-x); 0; inf)   = 1
        integrate(exp(-x^2); 0; 1)    = √π*erf(1)/2
        integrate(exp(-x^2); -inf; inf) = √π
        integrate(exp(x^2); 0; 1)     = √π*erfi(1)/2
        integrate(exp(-x^4); 0; 1)    ≈ 0.8448385947571024008

.. function:: lim(expression; point)
              lim(expression; variable; point)
              lim(expression; point; direction)
              lim(expression; variable; point; direction)

    Computes the limit of an expression as a variable approaches a
    specified value.

    If the expression has one variable, SpeedCrunch can infer it. Give a
    variable name if the expression has several. The other variables stay
    symbolic and act as real parameters, even if they have saved values.
    SpeedCrunch infers the variable from the expression alone. A defined
    constant expression gives its own value.

    Use ``-1`` to approach from below, ``1`` to approach from above, or
    ``0`` for both sides. The default is ``0``. The direction must be
    a dimensionless integer. If both three-argument forms fit, SpeedCrunch
    reports an ambiguity. Use all four arguments or write the point as an
    expression such as ``a+0``.

    Type ``inf`` or ``-inf`` for an infinite point. Use the default
    direction, or use ``-1`` for ``inf`` and ``1`` for ``-inf``.
    Trigonometric functions use the active angle mode. The point and
    limiting variable must have compatible units. The result keeps the
    expression's dimensions.

    Exact and infinite results use ``=``. A numerical estimate uses
    ``≈``. SpeedCrunch reports that the limit does not exist when it can
    establish that the requested approach fails, such as when the two
    sides disagree. It reports that it is unable to determine the limit
    when the rules or numerical estimates are inconclusive, including
    cases that need more information about a parameter.

    Limits follow real paths and the real domains of the functions in the
    expression. The expression can be undefined at the point itself.

    Examples in radians::

        lim(sin(x)/x; 0)                     = 1
        lim(abs(x)/x; 0; -1)                 = -1
        lim(1/x^2; 0)                       = inf
        lim(arctan(x); -inf)                 = -pi/2
        lim((1+1/x)^x; inf)                  = e
        lim((x^2-a^2)/(x-a); x; a)           = 2*a
        lim((gamma(2+x)-gamma(2))/x; 0)      ≈ 0.4227843351
