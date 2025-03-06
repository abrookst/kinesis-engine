import os
import sys
import subprocess

def compile_shaders(shader_dir):
    if not os.path.isdir(shader_dir):
        print(f"Error: Directory '{shader_dir}' does not exist.")
        return
    
    shader_extensions = ['.vert', '.frag', '.geom', '.comp', '.tesc', '.tese']
    
    for filename in os.listdir(shader_dir):
        file_path = os.path.join(shader_dir, filename)
        if os.path.isfile(file_path) and any(filename.endswith(ext) for ext in shader_extensions):
            output_path = file_path + ".spv"
            
            cmd = ["glslangValidator", "-V", file_path, "-o", output_path]
            
            try:
                subprocess.run(cmd, check=True)
                print(f"Compiled: {filename} -> {output_path}")
            except subprocess.CalledProcessError as e:
                print(f"Failed to compile {filename}: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python compile_shaders.py <shader_folder>")
        sys.exit(1)
    
    shader_folder = sys.argv[1]
    compile_shaders(shader_folder)