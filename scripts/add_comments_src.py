#!/usr/bin/env python3
"""
Script automatisé d'ajout de commentaires But/Entrées/Sortie pour les fonctions C++
Projet: routeur solaire (monRouteurSolaire)
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'src'

# Regex pour détecter les définitions de fonctions (classe::méthode ou fonction libre)
fn_re = re.compile(r'^\s*([\w:\~<>\*\&\s]+::\s*([A-Za-z0-9_~]+)\s*)\([^;]*\)\s*\{')

def has_but(lines, idx):
    """
    Vérifie si un commentaire contenant 'But:' existe avant la ligne idx
    Scanne jusqu'à 8 lignes au-dessus
    """
    i = idx - 1
    steps = 0
    while i >= 0 and steps < 8:
        l = lines[i].strip()
        if l == '':
            i -= 1
            steps += 1
            continue
        if 'But:' in l or 'But :'  in l:
            return True
        # Si on trouve un bloc commentaire fermant '*/', vérifier son contenu
        if l.endswith('*/'):
            j = i
            while j >= 0 and not lines[j].strip().startswith('/*'):
                if 'But:' in lines[j] or 'But :' in lines[j]:
                    return True
                j -= 1
            return False
        # Si autre ligne de code/commentaire sans 'But:', arrêter
        break
    return False

def make_comment(name):
    """
    Génère un bloc de commentaire par défaut avec But/Entrées/Sortie
    """
    return [
        '  /*',
        f'   * {name}',
        '   * But : (description automatique) — expliquer brièvement l\'objectif de la fonction',
        '   * Entrées : voir la signature de la fonction (paramètres)',
        '   * Sortie : valeur de retour ou effet sur l\'état interne',
        '   */'
    ]

def process_file(path: Path):
    """
    Parcourt le fichier et ajoute les commentaires manquants avant chaque fonction
    Crée un fichier .bak avant modification
    """
    text = path.read_text(encoding='utf-8')
    lines = text.splitlines()
    changed = False
    out = []
    i = 0
    while i < len(lines):
        m = fn_re.match(lines[i])
        if m:
            # Fonction détectée
            if not has_but(lines, i):
                # Pas de commentaire But: => ajouter
                out.extend(make_comment(m.group(1).strip()))
                changed = True
        out.append(lines[i])
        i += 1
    
    if changed:
        # Sauvegarder backup
        bak = path.with_suffix(path.suffix + '.bak')
        bak.write_text(text, encoding='utf-8')
        print(f'[INFO] Backup: {bak}')
        
        # Écrire le nouveau contenu
        path.write_text('\n'.join(out) + '\n', encoding='utf-8')
        print(f'[DONE] Modifié: {path}')
        return True
    else:
        print(f'[SKIP] Aucune modification: {path}')
        return False

def main():
    """Point d'entrée principal"""
    if not ROOT.exists():
        print(f'[ERROR] Répertoire src non trouvé: {ROOT}')
        return
    
    print(f'[START] Traitement du répertoire: {ROOT}')
    modified_count = 0
    
    for cpp_file in sorted(ROOT.glob('*.cpp')):
        print(f'\n=== Traitement: {cpp_file.name} ===')
        if process_file(cpp_file):
            modified_count += 1
    
    print(f'\n[SUMMARY] {modified_count} fichier(s) modifié(s)')

if __name__ == '__main__':
    main()
