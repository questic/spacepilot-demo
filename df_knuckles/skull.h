#ifndef _SKULL_H
#define _SKULL_H

#define SKULL_VERTICIES	699
#define SKULL_TRIANGLES	1243


Vertex SkullVerticies[SKULL_VERTICIES] =
{
	{-100,	-100,	100},
        {-100,	-100,	-100},
        {100,	-100,	-100},
        {100,	-100,	100},
        {-100,	100,	100},
        {100,	100,	100},
        {100,	100,	-100},
        {-100,	100,	-100}	
};


Triangle SkullTriangles[SKULL_TRIANGLES] = 
{
	{0,	1,	0},
	{1,	2,	0},
	{2,	3,	0},
	{3,	0,	0},
	{4,	5,	0},
	{5,	6,	0},
	{6,	7,	0},
	{7,	4,	0},
	{0,	4,	0},
	{1,	7,	0},
	{2,	6,	0},
	{3,	5,	0}
};


#endif /* _SKULL_H */
