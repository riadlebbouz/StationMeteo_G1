/* Modes de fonctionnement de la station meteo (diagramme d'etats du projet). */
#ifndef MODES_H
#define MODES_H

typedef enum
{
  MODE_STANDARD = 0,   /* LED verte  */
  MODE_CONFIGURATION,  /* LED jaune  */
  MODE_ECONOMIQUE,     /* LED bleue  */
  MODE_MAINTENANCE     /* LED orange */
} ModeSysteme;

void Modes_Init(void);
void Modes_Loop(void);
ModeSysteme Modes_Courant(void);

#endif
