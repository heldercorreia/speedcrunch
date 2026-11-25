.. meta::
   :description: Find symbolic derivatives and integrals in SpeedCrunch.

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
    exponential and logarithmic functions, and trigonometric and
    hyperbolic functions and their inverses.

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
    If there is no supported rule, the calculation gives an error.

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
    infinite interval. Type ``inf`` or ``-inf`` for an infinite bound.
    With automatic input adjustments on, the result display shows these
    bounds with ``∞``.
    Divergent integrals give an error even when opposite sides of a pole
    would cancel.

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
        integrate(exp(-x^2); 0; 1)    ≈ 0.7468241328124270254
