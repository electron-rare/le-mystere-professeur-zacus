import os
import sys
import yaml

# Usage: python gen_split_scenario.py <scenario_yaml> <output_dir>

def main():
    if len(sys.argv) != 3:
        print("Usage: python gen_split_scenario.py <scenario_yaml> <output_dir>")
        sys.exit(1)
    scenario_yaml = sys.argv[1]
    output_dir = sys.argv[2]
    with open(scenario_yaml, 'r') as f:
        scenario = yaml.safe_load(f)
    os.makedirs(output_dir, exist_ok=True)

    # Apps écran
    app_ids = set()
    for step in scenario.get('steps', []):
        for app in step.get('apps', []):
            app_ids.add(app)
    for app in app_ids:
        app_file = os.path.join(output_dir, f"app_{app.lower()}.yaml")
        with open(app_file, 'w') as f:
            f.write(f"id: {app}\nconfig:\n  # TODO: compléter la config\n")

    # Scènes
    scene_ids = set()
    for step in scenario.get('steps', []):
        scene = step.get('screen_scene_id')
        if scene:
            scene_ids.add(scene)
    for scene in scene_ids:
        scene_file = os.path.join(output_dir, f"scene_{scene.lower()}.yaml")
        with open(scene_file, 'w') as f:
            f.write(f"id: {scene}\ncontent:\n  # TODO: compléter le contenu\n")

    print(f"Génération terminée dans {output_dir}")

if __name__ == "__main__":
    main()
