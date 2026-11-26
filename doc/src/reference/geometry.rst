.. meta::
   :description: Calculate hypotenuses, areas, and volumes for polygons,
                 solids, tanks, and triangle meshes in SpeedCrunch.

Geometry
========

Use ``area_`` for flat shapes and for the total surface area of solids.
Use ``volume_`` for volume. The ``_lateral`` suffix gives the side or curved
area. For a composite solid, add the volumes of its non-overlapping
parts and count only its exposed faces for surface area.

Enter measurements as plain numbers in a common unit, or give each
measurement a unit. Physical lengths can use different units, such as
meters and centimeters. Results have square or cubic units when the
inputs carry units. Angles without units follow the current angle mode.
An explicit unit such as ``[deg]`` works in any mode. Heights are
perpendicular to the base. Use positive lengths and areas unless the
function gives a range that includes zero or negative values.

For shapes given by coordinates, enter a matrix with one point or
vector per row. The optional scale is the length of one coordinate
unit, for example ``1[cm]``. With no scale, the result is a plain
number.

.. _geometry-exact-results:

.. rubric:: Exact results

With any decimal notation, SpeedCrunch adds a line below the decimal
result when it finds a simple exact form for a Geometry calculation.
:menuselection:`Rational` uses that form on its result line. This also works
in arithmetic such as ``2 * area_circle(2)``.

The display recognizes fractions such as ``1 / 6``, multiples of π or π²,
square roots, and some sums and differences. The exact form can help you
check your work against a formula or reuse the value without rounding.
For example, in
``area_rectangular_pyramid_lateral(8; 6; 4)``, one pair of side faces
contributes ``40`` and the other contributes ``24 · √(2)``.

SpeedCrunch checks the calculated value before rounding it for display.
For example, a sector of 90 degrees with radius 1 matches ``π / 4``.
A sector just under 90 degrees can round to the same decimal and show
only the decimal line.

Lengths
-------

.. function:: hypot(x1; x2; ...)

    .. versionadded:: 1.2

    Computes the square root of the sum of the squares of its arguments.
    Use two or more real values with compatible dimensions. The result
    keeps those dimensions. If any value is ``inf`` or ``-inf`` and the
    other values are valid, the result is ``inf``.

    Examples::

        hypot(3; 4)          = 5
        hypot(1; 1; 1)       = sqrt(3)
        hypot(3[m]; 400[cm]) = 5[m]

Plane shapes
------------

.. function:: area_square(side)

    Square the side length.

    Example::

        area_square(4)
        = 16

.. function:: area_rectangle(length; width)

    Multiply the length by the width.

    Example::

        area_rectangle(4; 3)
        = 12

.. function:: area_triangle(base; height)

    Multiply the base by the perpendicular height and divide by 2. For a right
    triangle, use the two legs.

    Example::

        area_triangle(4; 3)
        = 6

.. function:: area_triangle_from_sides(a; b; c)

    Use Heron's formula, √(s(s − a)(s − b)(s − c)), where
    s = (a + b + c) / 2. The sides must form a triangle.

    Example::

        area_triangle_from_sides(3; 4; 5)
        = 6

.. function:: area_triangle_from_angle(a; b; angle)

    Use ab sin(θ) / 2, where θ is the angle between
    the two sides. It must be between 0 and 180 degrees.

    Example::

        area_triangle_from_angle(3; 4; 90[deg])
        = 6

.. function:: area_parallelogram(base; height)

    Multiply the base by the perpendicular height.

    Example::

        area_parallelogram(5; 3)
        = 15

.. function:: area_trapezoid(base_1; base_2; height)

    Average the lengths of the parallel bases and multiply by the
    perpendicular height. This shape is also called a trapezium.

    Example::

        area_trapezoid(3; 7; 2)
        = 10

.. function:: area_rhombus(diagonal_1; diagonal_2)

    Multiply the diagonal lengths and divide by 2.

    Example::

        area_rhombus(4; 6)
        = 12

.. function:: area_kite(diagonal_1; diagonal_2)

    Multiply the diagonal lengths and divide by 2.

    Example::

        area_kite(5; 8)
        = 20

.. function:: area_regular_polygon(n; side)

    Use n s² / (4 tan(π / n)) for side length s and a whole
    number of sides n ≥ 3. This covers equilateral triangles and
    regular polygons of any size.

    Example::

        area_regular_polygon(4; 3)
        = 9

.. function:: area_polygon(vertices [; scale])

    Use the shoelace formula. For each edge from (x₁, y₁) to (x₂, y₂),
    compute x₁y₂ − y₁x₂. Add these values, take the absolute value, and
    divide by 2. Enter each vertex as an ``{x; y}`` row in boundary order,
    clockwise or counterclockwise. The last vertex connects to the first.
    Edges may meet at their shared endpoints. You can repeat the first
    vertex at the end.

    Example::

        area_polygon({{0; 0}; {4; 0}; {4; 3}; {0; 3}}; 1[m])
        = 12 m²

.. function:: area_circle(radius)

    Use πr². For a diameter, pass half of it as the radius.

    Example::

        area_circle(2)
        = 4pi

.. function:: area_circular_sector(radius; angle)

    Use r²θ / 2, with the sweep angle θ converted to
    radians. Use 90 degrees for a quarter circle and 180 degrees for a
    semicircle. The sweep can range from 0 to 360 degrees.

    Example::

        area_circular_sector(2; 90[deg])
        = pi

.. function:: area_circular_segment(radius; depth)

    Measure the depth from the chord to the arc along the perpendicular
    through the center. Depth ranges from zero to the diameter. Use the
    segment formula below with
    θ = 4 arcsin(√(depth / (2r))).

    Example::

        area_circular_segment(2; 2)
        = 2pi

.. function:: area_circular_segment_from_angle(radius; angle)

    Use r²(θ − sin(θ)) / 2, with the central angle
    θ converted to radians. The angle can range from 0 to 360
    degrees.

    Example::

        area_circular_segment_from_angle(2; 180[deg])
        = 2pi

.. function:: area_ellipse(semi_axis_1; semi_axis_2)

    Multiply the two semiaxes by π. A semiaxis is half of the full
    axis length.

    Example::

        area_ellipse(3; 2)
        = 6pi

.. function:: area_annulus(outer_radius; inner_radius)

    Use π(R² − r²) for a ring with inner radius 0 ≤ r ≤ R.

    Example::

        area_annulus(3; 2)
        = 5pi

.. function:: area_annular_sector(outer_radius; inner_radius; angle)

    Use (R² − r²)θ / 2, with the sweep converted to radians. The
    inner radius can range from zero to the outer radius. The sweep can range
    from 0 to 360 degrees.

    Example::

        area_annular_sector(3; 2; 180[deg])
        = 2.5pi

.. function:: area_rounded_rectangle(length; width; corner_radius)

    Use lw − (4 − π)r² for four equally rounded corners. The corner
    radius is at most half the shorter side.

    Example::

        area_rounded_rectangle(4; 2; 1)
        = 4 + pi

.. function:: area_stadium(straight_length; radius)

    Add the area of the rectangular middle to that of the two semicircles,
    2rL + π r². The straight length L is the distance between
    the arc centers. This shape is also called an obround.

    Example::

        area_stadium(3; 1)
        = 6 + pi

.. function:: area_parabolic_segment(chord; rise)

    Use 2bh / 3 for a chord of length b perpendicular to the
    symmetry axis. Measure the rise h from the chord to the vertex.
    A rise of zero gives zero area.

    Example::

        area_parabolic_segment(6; 2)
        = 8

.. function:: area_circle_overlap(radius_1; radius_2; center_distance)

    Measure the separation between the centers. The overlap is the sum of two
    circular segments in the partial case. Circles that only touch have zero
    overlap. If one circle contains the other, use the smaller circle's area.

    Example::

        area_circle_overlap(2; 1; 1)
        = pi

Prisms and pyramids
-------------------

.. function:: area_cube(edge)

    Add the areas of all six square faces, 6s².

    Example::

        area_cube(2)
        = 24

.. function:: area_cube_lateral(edge)

    Add the areas of the four side faces, 4s².

    Example::

        area_cube_lateral(2)
        = 16

.. function:: volume_cube(edge)

    Cube the edge length.

    Example::

        volume_cube(3)
        = 27

.. function:: area_cuboid(length; width; height)

    Use 2(lw + lh + wh) for a rectangular prism.

    Example::

        area_cuboid(4; 3; 2)
        = 52

.. function:: area_cuboid_lateral(length; width; height)

    Use 2(l + w)h for the four side faces around the length and width of
    the base.

    Example::

        area_cuboid_lateral(4; 3; 2)
        = 28

.. function:: volume_cuboid(length; width; height)

    Multiply length, width, and height.

    Example::

        volume_cuboid(4; 3; 2)
        = 24

.. function:: area_prism(base_area; base_perimeter; height)

    Use 2A + Ph, where A and P are the area and
    perimeter of one base.

    Example::

        area_prism(6; 12; 4)
        = 60

.. function:: area_prism_lateral(base_perimeter; height)

    Multiply the base perimeter by the height.

    Example::

        area_prism_lateral(12; 4)
        = 48

.. function:: volume_prism(base_area; height)

    Multiply the base area by the perpendicular height. This also works for
    oblique prisms.

    Example::

        volume_prism(6; 4)
        = 24

.. function:: area_triangular_prism(a; b; c; height)

    Find the area of the triangular base from its three sides and use
    :func:`area_prism`. The height is the distance between the triangular faces.

    Example::

        area_triangular_prism(3; 4; 5; 4)
        = 60

.. function:: area_triangular_prism_lateral(a; b; c; height)

    Multiply the sum of the three base sides by the prism's height.

    Example::

        area_triangular_prism_lateral(3; 4; 5; 4)
        = 48

.. function:: volume_triangular_prism(a; b; c; height)

    Multiply the base area from Heron's formula by the prism's height.

    Example::

        volume_triangular_prism(3; 4; 5; 4)
        = 24

.. function:: area_regular_prism(n; side; height)

    Add both regular polygon bases and the sides. Use
    :func:`area_regular_polygon` for the base area.

    Example::

        area_regular_prism(4; 3; 2)
        = 42

.. function:: area_regular_prism_lateral(n; side; height)

    Multiply the height by the regular polygon's perimeter ns.

    Example::

        area_regular_prism_lateral(4; 3; 2)
        = 24

.. function:: volume_regular_prism(n; side; height)

    Multiply the area of the regular polygon base by the height.

    Example::

        volume_regular_prism(4; 3; 2)
        = 18

.. function:: volume_pyramid(base_area; height)

    Multiply the base area by the perpendicular height and divide by 3.

    Example::

        volume_pyramid(16; 6)
        = 32

.. function:: area_square_pyramid(side; height)

    Add the square base to the four triangular sides. The apex is above the
    center of the base.

    Example::

        area_square_pyramid(6; 4)
        = 96

.. function:: area_square_pyramid_lateral(side; height)

    Use 2s√(h² + (s / 2)²) for the four triangular sides.

    Example::

        area_square_pyramid_lateral(6; 4)
        = 60

.. function:: volume_square_pyramid(side; height)

    Use s²h / 3.

    Example::

        volume_square_pyramid(6; 4)
        = 48

.. function:: area_rectangular_pyramid(length; width; height)

    Add the rectangular base to the four triangular sides. The apex is above
    the center of the base.

    Example::

        area_rectangular_pyramid(6; 6; 4)
        = 96

.. function:: area_rectangular_pyramid_lateral(length; width; height)

    Use l√(h² + (w / 2)²) + w√(h² + (l / 2)²). Each pair of opposite
    faces shares a slant height.

    Example::

        area_rectangular_pyramid_lateral(8; 6; 4)
        = 73.94112549695428117124
        = 40 + 24 · √(2)

.. function:: volume_rectangular_pyramid(length; width; height)

    Use lwh / 3.

    Example::

        volume_rectangular_pyramid(8; 6; 4)
        = 64

.. function:: area_regular_pyramid(n; side; height)

    Add the regular polygon base and the lateral area. The apex is above the
    center of the base.

    Example::

        area_regular_pyramid(4; 6; 4)
        = 96

.. function:: area_regular_pyramid_lateral(n; side; height)

    Multiply half the base perimeter by √(h² + a²), where
    a is the distance from the base center to a side.

    Example::

        area_regular_pyramid_lateral(4; 6; 4)
        = 60

.. function:: volume_regular_pyramid(n; side; height)

    Multiply the regular polygon's area by the height and divide by 3.

    Example::

        volume_regular_pyramid(4; 6; 4)
        = 48

.. function:: area_regular_pyramid_frustum(n; side_1; side_2; height)

    Add both regular polygon bases and the lateral area between them. A side
    length of zero gives a pyramid.

    Example::

        area_regular_pyramid_frustum(4; 6; 0; 4)
        = 96

.. function:: area_regular_pyramid_frustum_lateral(n; side_1; side_2; height)

    Average the two base perimeters and multiply by the slant height. The
    slant height is computed from the perpendicular height and the difference
    between the two base apothems.

    Example::

        area_regular_pyramid_frustum_lateral(4; 6; 6; 4)
        = 96

.. function:: volume_regular_pyramid_frustum(n; side_1; side_2; height)

    Compute the two regular polygon base areas and use
    :func:`volume_pyramid_frustum`. At least one side length must be positive.

    Example::

        volume_regular_pyramid_frustum(4; 6; 3; 4)
        = 84

.. function:: volume_pyramid_frustum(base_area_1; base_area_2; height)

    Use h(A₁ + A₂ + √(A₁ A₂)) / 3 for two parallel cuts of a
    pyramid. A base area of zero gives the tip.

    Example::

        volume_pyramid_frustum(36; 9; 4)
        = 84

.. function:: volume_prismoid(end_area_1; mid_area; end_area_2; height)

    Use h(A₁ + 4Aₘ + A₂) / 6. Measure the middle area halfway between the
    parallel ends. This formula is exact when the cross-sectional area varies
    quadratically or less with height.

    Example::

        volume_prismoid(9; 16; 25; 3)
        = 49

Cylinders, cones and rounded solids
-----------------------------------

.. function:: area_cylinder(radius; height)

    Use 2π r² + 2π rh for a right circular cylinder, including both
    ends.

    Example::

        area_cylinder(2; 3)
        = 20pi

.. function:: area_cylinder_lateral(radius; height)

    Use 2π rh for the curved wall.

    Example::

        area_cylinder_lateral(2; 3)
        = 12pi

.. function:: volume_cylinder(radius; height)

    Use π r² h.

    Example::

        volume_cylinder(10[cm]; 20[cm]) -> [L]
        = 2pi L

.. function:: volume_cylindrical_segment(radius; length; depth)

    Multiply the circular segment area by the cylinder length. For a
    horizontal cylinder with flat ends, depth is the liquid height above the
    inside bottom. Its range is zero to the diameter.

    Example::

        volume_cylindrical_segment(2; 4; 2)
        = 8pi

.. function:: area_cone(radius; height)

    Add the circular base and curved wall of a right circular cone.

    Example::

        area_cone(3; 4)
        = 24pi

.. function:: area_cone_lateral(radius; height)

    Use π r√(r² + h²) for the curved wall.

    Example::

        area_cone_lateral(3; 4)
        = 15pi

.. function:: volume_cone(radius; height)

    Use π r² h / 3.

    Example::

        volume_cone(3; 4)
        = 12pi

.. function:: area_cone_frustum(radius_1; radius_2; height)

    Add the two circular ends and the curved wall of a right circular cone
    frustum. A radius of zero gives the tip.

    Example::

        area_cone_frustum(3; 0; 4)
        = 24pi

.. function:: area_cone_frustum_lateral(radius_1; radius_2; height)

    Use π(r₁ + r₂)√(h² + (r₁ − r₂)²) for the curved wall. At
    least one radius must be positive.

    Example::

        area_cone_frustum_lateral(3; 3; 4)
        = 24pi

.. function:: volume_cone_frustum(radius_1; radius_2; height)

    Use π h(r₁² + r₁r₂ + r₂²) / 3. Enter the two radii in either
    order.

    Example::

        volume_cone_frustum(3; 2; 3)
        = 19pi

.. function:: area_hollow_cylinder(outer_radius; inner_radius; height)

    Add the inner wall, the outer wall, and both ring-shaped ends. The inner
    radius must be smaller than the outer radius.

    Example::

        area_hollow_cylinder(3; 2; 4)
        = 50pi

.. function:: area_hollow_cylinder_lateral(outer_radius; inner_radius; height)

    Use 2π(R + r)h for the inner and outer walls.

    Example::

        area_hollow_cylinder_lateral(3; 2; 4)
        = 40pi

.. function:: volume_hollow_cylinder(outer_radius; inner_radius; height)

    Multiply the area of the ring-shaped cross section by the height to get
    the material volume.

    Example::

        volume_hollow_cylinder(3; 2; 4)
        = 20pi

.. function:: area_sphere(radius)

    Use 4π r².

    Example::

        area_sphere(2)
        = 16pi

.. function:: volume_sphere(radius)

    Use 4π r³ / 3.

    Example::

        volume_sphere(3)
        = 36pi

.. function:: area_hemisphere(radius)

    Add the flat disk and curved face, 3π r².

    Example::

        area_hemisphere(2)
        = 12pi

.. function:: area_hemisphere_lateral(radius)

    Use 2π r² for the curved face.

    Example::

        area_hemisphere_lateral(2)
        = 8pi

.. function:: volume_hemisphere(radius)

    Use 2π r³ / 3.

    Example::

        volume_hemisphere(3)
        = 18pi

.. function:: area_spherical_cap(radius; height)

    Add the curved face and the flat disk of area π h(2r − h). Measure
    height from the cut plane to the tip. It can range from zero to the
    diameter.

    Example::

        area_spherical_cap(2; 2)
        = 12pi

.. function:: area_spherical_cap_lateral(radius; height)

    Use 2π rh for the curved face.

    Example::

        area_spherical_cap_lateral(2; 2)
        = 8pi

.. function:: volume_spherical_cap(radius; height)

    Use π h²(r − h / 3), with height measured from the cut plane to the
    tip.

    Example::

        volume_spherical_cap(2; 1)
        = 5pi/3

.. function:: area_spherical_segment(radius; lower_z; upper_z)

    Add the curved surface and the two cut disks. Enter the signed positions
    of the lower and upper planes relative to the sphere's center, in
    ascending order and within the sphere.

    Example::

        area_spherical_segment(2; -1; 1)
        = 14pi

.. function:: area_spherical_segment_lateral(radius; lower_z; upper_z)

    Use 2π r(z₂ − z₁) for the curved surface between two cuts.

    Example::

        area_spherical_segment_lateral(2; -1; 1)
        = 8pi

.. function:: volume_spherical_segment(radius; lower_z; upper_z)

    Take the volume between the two cuts. The section at position z
    has area π(r² − z²), so :func:`volume_prismoid` also gives the
    volume.

    Example::

        volume_spherical_segment(2; -1; 1)
        = 22pi/3

.. function:: area_spherical_shell(outer_radius; inner_radius)

    Add the areas of the inner and outer spheres. The inner radius must be
    smaller than the outer radius.

    Example::

        area_spherical_shell(3; 2)
        = 52pi

.. function:: volume_spherical_shell(outer_radius; inner_radius)

    Subtract the inner sphere volume from the outer sphere volume to get the
    material volume.

    Example::

        volume_spherical_shell(3; 2)
        = 76pi/3

.. function:: area_capsule(radius; straight_length)

    Add the cylinder's curved wall and two hemispheres. Straight length is the
    length of the cylinder between the hemispheres.

    Example::

        area_capsule(2; 3)
        = 28pi

.. function:: volume_capsule(radius; straight_length)

    Add the cylinder and sphere volumes, π r² L + 4π r³ / 3.

    Example::

        volume_capsule(3; 2)
        = 54pi

.. function:: area_torus(major_radius; minor_radius)

    Use 4π² Rr. The major radius goes from the torus center to the
    center of the tube, and must exceed the tube radius.

    Example::

        area_torus(3; 1)
        = 12pi^2

.. function:: volume_torus(major_radius; minor_radius)

    Use 2π² Rr².

    Example::

        volume_torus(3; 1)
        = 6pi^2

.. function:: area_ellipsoid(semi_axis_1; semi_axis_2; semi_axis_3)

    Use Carlson's symmetric elliptic integral
    4π R_G((bc)²,(ac)²,(ab)²) for semiaxes a, b, c.

    Example::

        area_ellipsoid(3; 2; 1)
        = about 48.882146

.. function:: volume_ellipsoid(semi_axis_1; semi_axis_2; semi_axis_3)

    Use 4π abc / 3.

    Example::

        volume_ellipsoid(3; 2; 1)
        = 8pi

.. function:: area_spheroid(equatorial_radius; polar_radius)

    Use :func:`area_ellipsoid` with both equatorial semiaxes set to the
    equatorial radius.

    Example::

        area_spheroid(2; 2)
        = 16pi

.. function:: volume_spheroid(equatorial_radius; polar_radius)

    Use 4π a²c / 3 for equatorial radius a and polar radius
    c.

    Example::

        volume_spheroid(3; 2)
        = 24pi

.. function:: area_elliptical_cylinder(semi_axis_1; semi_axis_2; height)

    Add both elliptical ends and the curved wall of a right elliptical
    cylinder.

    Example::

        area_elliptical_cylinder(2; 2; 3)
        = 20pi

.. function:: area_elliptical_cylinder_lateral(semi_axis_1; semi_axis_2; height)

    Multiply the perimeter of the ellipse by the height. For a ≥ b,
    the perimeter is 8R_G(0,b²,a²).

    Example::

        area_elliptical_cylinder_lateral(3; 2; 1)
        = about 15.865440

.. function:: volume_elliptical_cylinder(semi_axis_1; semi_axis_2; height)

    Multiply the area π ab of one elliptical end by the height.

    Example::

        volume_elliptical_cylinder(3; 2; 4)
        = 24pi

Polyhedra and meshes
--------------------

.. function:: area_regular_tetrahedron(edge)

    Add the four equilateral triangles to get √3s².

    Example::

        area_regular_tetrahedron(2)
        = 4sqrt(3)

.. function:: volume_regular_tetrahedron(edge)

    Use √2s³ / 12.

    Example::

        volume_regular_tetrahedron(2)
        = 2sqrt(2)/3

.. function:: area_regular_octahedron(edge)

    Add the eight equilateral triangles to get 2√3s².

    Example::

        area_regular_octahedron(2)
        = 8sqrt(3)

.. function:: volume_regular_octahedron(edge)

    Use √2s³ / 3.

    Example::

        volume_regular_octahedron(2)
        = 8sqrt(2)/3

.. function:: area_regular_dodecahedron(edge)

    Add the twelve regular pentagons to get 3√(25 + 10√5)s².

    Example::

        area_regular_dodecahedron(1)
        = about 20.645729

.. function:: volume_regular_dodecahedron(edge)

    Use (15 + 7√5)s³ / 4.

    Example::

        volume_regular_dodecahedron(1)
        = 7.66311896062463196872
        = (15 + 7 · √(5)) / 4

.. function:: area_regular_icosahedron(edge)

    Add the twenty equilateral triangles to get 5√3s².

    Example::

        area_regular_icosahedron(2)
        = 20sqrt(3)

.. function:: volume_regular_icosahedron(edge)

    Use 5(3 + √5)s³ / 12.

    Example::

        volume_regular_icosahedron(1)
        = about 2.181695

.. function:: area_tetrahedron(vertices [; scale])

    Enter four ``{x; y; z}`` rows for the vertices. Add the areas of the four
    triangular faces. Each face area is half the length of the cross product
    of two edges.

    Example::

        area_tetrahedron({{0; 0; 0}; {1; 0; 0}; {0; 1; 0}; {0; 0; 1}})
        = 2.36602540378443864676
        = (3 + √(3)) / 2

.. function:: volume_tetrahedron(vertices [; scale])

    Use one sixth of the absolute scalar triple product of three edges from
    the same vertex.

    Example::

        volume_tetrahedron({{0; 0; 0}; {1; 0; 0}; {0; 1; 0}; {0; 0; 1}})
        = 1/6

.. function:: area_parallelepiped(edges [; scale])

    Provide three edge vectors from one corner, one per row. Sum the lengths
    of the three pairwise cross products and double the result.

    Example::

        area_parallelepiped({{4; 0; 0}; {0; 3; 0}; {0; 0; 2}})
        = 52

.. function:: volume_parallelepiped(edges [; scale])

    Use the absolute scalar triple product of three edge vectors from one
    corner.

    Example::

        volume_parallelepiped({{4; 0; 0}; {0; 3; 0}; {0; 0; 2}})
        = 24

.. function:: area_mesh(vertices; faces [; scale])

    Sum the areas of the listed triangular faces. Give the vertices as rows of
    three coordinates and the faces as rows of three vertex numbers, starting
    at 1. The surface can be open.

    Example::

        area_mesh({{0; 0; 0}; {1; 0; 0}; {0; 1; 0}; {0; 0; 1}}; {{1; 2; 3}})
        = 0.5

.. function:: volume_mesh(vertices; faces [; scale])

    Use the absolute sum of signed tetrahedron volumes, one per face. Faces
    must form one closed shell with consistent winding and no crossing faces.
    Each edge must occur twice, in opposite directions.

    Example::

        volume_mesh({{0; 0; 0}; {1; 0; 0}; {0; 1; 0}; {0; 0; 1}}; {{1; 3; 2}; {1; 2; 4}; {1; 4; 3}; {2; 3; 4}})
        = 1/6

.. seealso::

    `NIST reference on ellipsoid surface area <https://dlmf.nist.gov/19.33>`_
    and `ellipse perimeter <https://dlmf.nist.gov/19.30>`_.
