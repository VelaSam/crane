# Grue_ProjetMultisciplinaire
Systèmes Embarqués
Code pour le Projet Multidisciplinaire. Un fichier contenant le programme de la grue et un autre fichier contenant le programme de la manette

Le fichier de manette s'occupe de la logique. Elle prends constamment le input de l'utilisateur (quel moteur qu'il faut tourner, quelle direction, quelle vitesse,
mise en mode automatique ou manuelle, etc...) et ensuite elle met toute l'information sous forme de string qui sera envoyée par module WiFi au PCB sur la grue.
 
Le PCB de la grue recoit le string et ensuite il split les informations et ensuite il envoit le courant respectif à la broche respective pour faire activer les moteurs
