#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <directfb.h>

#include "matrix.h"
#include "skull.h"
#include "spnav.h"
#include "spnav.c"
#include "spnav_config.h"
#include "spnav_magellan.h"
#include "spnav_magellan.c"
#include "vmath.h"


#undef  CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define SQ(x)	((x) * (x))

IDirectFB            *dfb;
IDirectFBSurface     *primary;
IDirectFBEventBuffer *event_buffer;


Bool    BackfaceCull  = False;
Bool    DoubleBuffer  = True;
Bool    Lighting      = False;
int     PrimitiveType = WIRE_FRAME;
float   ScaleFactor   = 0.0012;

int Width, Height;

Vertex Light1 = {0.0,  0.0, 1.0};
Vertex Light2 = {0.2, -0.2, 0.4};

vec3_t pos = {0, 0, -6};
quat_t rot = {0, 0, 0, 1};
int redisplay;
spnav_event spev;

static Tri3D Triangles[SKULL_TRIANGLES];
static Vertex TransformedVerticies[SKULL_VERTICIES];


static void ClearBuffer (void)
{
  primary->SetColor (primary, 0, 0, 0, 0);
  primary->FillRectangle (primary, 0, 0, Width, Height);
}

static void DrawTriangle (float light1, float light2, Tri3D* tri)
{
  u8 r, g, b;
  int X, Y;

  X = Width >> 1;
  Y = Height >> 1;

  r = light1 * 255.0;
  g = (light1 * light1) * 255.0;
  b = light1 * 64.0 + light2 * 64.0;

  primary->SetColor (primary, r, g, b, 0xff);

  switch (PrimitiveType)
    {
    case FLAT_SHADED:
      primary->FillTriangle (primary,
                             tri->a->x + X, tri->a->y + Y,
                             tri->b->x + X, tri->b->y + Y,
                             tri->c->x + X, tri->c->y + Y);
      break;

    case WIRE_FRAME:
      primary->DrawLine (primary,
                         tri->a->x + X, tri->a->y + Y,
                         tri->b->x + X, tri->b->y + Y);
      primary->DrawLine (primary,
                         tri->b->x + X, tri->b->y + Y,
                         tri->c->x + X, tri->c->y + Y);
      primary->DrawLine (primary,
                         tri->c->x + X, tri->c->y + Y,
                         tri->a->x + X, tri->a->y + Y);
      break;

    default:
      break;
    }
}

static void DrawIt (void)
{
  int count, NumUsed = 0;
  Tri3D *current = Triangles;
  Tri3D *first = Triangles;
  Tri3D *pntr, *prev;
  Triangle *points = SkullTriangles;
  Vertex *transPoints = TransformedVerticies;
  Vertex *untransPoints = SkullVerticies;
  Vertex A, B;
  float length;
  float light1, light2;

  ClearBuffer();

  count = SKULL_VERTICIES;
  while(count--)
    MultiplyVector(untransPoints++, transPoints++);

  first->next = NULL;

  count = SKULL_TRIANGLES;
  while(count--)
    {
      current->a = TransformedVerticies + points->a;
      current->b = TransformedVerticies + points->b;
      current->c = TransformedVerticies + points->c;

      A.x = current->b->x - current->a->x;
      A.y = current->b->y - current->a->y;
      A.z = current->b->z - current->a->z;

      B.x = current->c->x - current->b->x;
      B.y = current->c->y - current->b->y;
      B.z = current->c->z - current->b->z;

      current->normal.z = (A.x * B.y) - (A.y * B.x);

      if(BackfaceCull && (current->normal.z >= 0.0))
        {
          points++;
          continue;
        }

      current->normal.y = (A.z * B.x) - (A.x * B.z);
      current->normal.x = (A.y * B.z) - (A.z * B.y);

      current->depth = current->a->z + current->b->z + current->c->z;

      /* Not the smartest sorting algorithm */
      if(NumUsed)
        {
          prev = NULL;
          pntr = first;
          while(pntr)
            {
              if(current->depth > pntr->depth)
                {
                  if(pntr->next)
                    {
                      prev = pntr;
                      pntr = pntr->next;
                    }
                  else
                    {
                      pntr->next = current;
                      current->next = NULL;
                      break;
                    }
                }
              else
                {
                  if(prev)
                    {
                      prev->next = current;
                      current->next = pntr;
                    }
                  else
                    {
                      current->next = pntr;
                      first = current;
                    }
                  break;
                }
            }
        }

      NumUsed++;
      current++;
      points++;
    }

  while(first)
    {
      if(Lighting)
        {
          length = ((first->normal.x * first->normal.x) +
                    (first->normal.y * first->normal.y) +
                    (first->normal.z * first->normal.z));

          length = (float)sqrt((double)length);

          light1 = -((first->normal.x * Light1.x) +
                     (first->normal.y * Light1.y) +
                     (first->normal.z * Light1.z)) / length;
          light1 = CLAMP (light1, 0.0, 1.0);

          light2 = abs((first->normal.x * Light2.x) +
                       (first->normal.y * Light2.y) +
                       (first->normal.z * Light2.z)) / length;
          light2 = CLAMP (light2, 0.0, 1.0);
        }
      else
        {
          light1 = 1.0;
          light2 = 0.0;
        }

      DrawTriangle(light1, light2, first);

      first = first->next;
    }
}

void redraw(void)
{
	mat4_t xform;

	quat_to_mat(xform, rot);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(pos.x, pos.y, pos.z);
	glMultTransposeMatrixf((float*)xform);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);	

	glLineWidth(4); 
    	glColor3f(0.0f, 2.0f, 0.0f); 
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
    	DrawIt();

    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();
	
 
    	glRotated(90, 0, 1, 0);
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPushMatrix();
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();
 
    	glRotated(-90, 1, 0, 0);
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPushMatrix();
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
    	DrawIt();

	glRotated(-180, 1, 0, 0);
		
	glColor3f(0.0f, 2.0f, 0.0f);
    	glPushMatrix();
	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
    	DrawIt();
	
	glRotated(-270, 1, 0, 0);
		
	glColor3f(0.0f, 2.0f, 0.0f);
    	glPushMatrix();
	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
    	DrawIt();

	glRotated(360, 1, 0, 1);
		
	glColor3f(0.0f, 2.0f, 0.0f);
    	glPushMatrix();
	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
    	DrawIt();
	
	glRotated(180, 1, 0, 1);
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPushMatrix();
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();
 
    	glColor3f(0.0f, 2.0f, 0.0f);
    	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE); 
    	DrawIt();

	//glXSwapBuffers(dpy, win);
}

static int SetupDirectFB (int argc, char *argv[])
{
  DFBResult ret;
  DFBSurfaceDescription dsc;

  ret = DirectFBInit (&argc, &argv);
  if (ret)
    {
      DirectFBError ("DirectFBInit failed", ret);
      return -1;
    }

  ret = DirectFBCreate (&dfb);
  if (ret)
    {
      DirectFBError ("DirectFBCreate failed", ret);
      return -2;
    }

  dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN);

  ret = dfb->CreateInputEventBuffer (dfb, DICAPS_KEYS | DICAPS_AXES,
                                     DFB_FALSE, &event_buffer);
  if (ret)
    {
      DirectFBError ("CreateInputBuffer for failed", ret);
      dfb->Release (dfb);
      return -4;
    }

  dsc.flags = DSDESC_CAPS;
  dsc.caps  = DSCAPS_PRIMARY | (DoubleBuffer ? DSCAPS_DOUBLE : 0);

  ret = dfb->CreateSurface (dfb, &dsc, &primary);
  if (ret)
    {
      DirectFBError ("CreateSurface for primary failed", ret);
      event_buffer->Release (event_buffer);
      dfb->Release (dfb);
      return -7;
    }
if(spnav_open()==-1) {
	  	DirectFBError ("failed to connect to the space navigator daemon", ret);
		return 1;
	}

  primary->GetSize (primary, &Width, &Height);

  return 0;
}

static void ClosedownDirectFB (void)
{
  primary->Release (primary);
  event_buffer->Release (event_buffer);
  dfb->Release (dfb);
}

int main (int argc, char *argv[])
{
  int quit = False;
  int dxL, dyL;
	
  if(SetupDirectFB (argc, argv))
    return -1;

  ScaleFactor *= Height;

  InitMatrix();
  SetupMatrix(ScaleFactor);

  DrawIt();
  if(DoubleBuffer)
    primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC);

  dxL = 11;
  dyL = 7;

  while(!quit)
    {
	if(spev.type == SPNAV_EVENT_MOTION) {
				/* apply axis/angle rotation to the quaternion */
				if(spev.motion.rx || spev.motion.ry || spev.motion.rz) {
					float axis_len = sqrt(SQ(spev.motion.rx) + SQ(spev.motion.ry) + SQ(spev.motion.rz));
					rot = quat_rotate(rot, axis_len * 0.001, -spev.motion.rx / axis_len,
							-spev.motion.ry / axis_len, spev.motion.rz / axis_len);
				}

				/* add translation */
				pos.x += spev.motion.x * 0.001;
				pos.y += spev.motion.y * 0.001;
				pos.z -= spev.motion.z * 0.001;

				redisplay = 1;
			} else {
				/* on button press, reset the cube */
				if(spev.button.press) {
					pos = v3_cons(0, 0, -6);
					rot = quat_cons(1, 0, 0, 0);

					redisplay = 1;
				}
			}
			/* finally remove any other queued motion events */
			spnav_remove_events(SPNAV_EVENT_MOTION);
      DFBInputEvent evt;

      while (event_buffer->GetEvent (event_buffer, DFB_EVENT(&evt)) == DFB_OK)
        {
          if (evt.type == DIET_AXISMOTION && evt.flags & DIEF_AXISREL)
            {
              if (evt.axis == DIAI_X)
                Rotate (evt.axisrel * 2, 'y');
              else if (evt.axis == DIAI_Y)
                Rotate (-evt.axisrel * 2, 'x');
            }
          else if (evt.type == DIET_KEYPRESS)
            {
              switch (evt.key_symbol)
                {
                case DIKS_OK:
                case DIKS_SPACE:
                  if (PrimitiveType == FLAT_SHADED)
                    PrimitiveType = WIRE_FRAME;
                  else
                    PrimitiveType = FLAT_SHADED;
                  break;

                case DIKS_ESCAPE:
                case DIKS_POWER:
                  quit = True;
                  break;

                default:
                  ;
                }
              
              switch (evt.key_id)
                {
                case DIKI_B:
                  BackfaceCull = !BackfaceCull;
                  break;

                case DIKI_Q:
                  quit = True;
                  break;

                default:
                  ;
                }
            }
        }

      if (rand()%50 == 0)
        dxL += rand()%5 - 2;
      if (rand()%50 == 0)
        dyL += rand()%5 - 2;

      if(dxL | dyL)
        RotateLight(&Light1, dxL, dyL);

      DrawIt();
      if(DoubleBuffer)
        primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC);
    }

  ClosedownDirectFB();

  return 0;
}
