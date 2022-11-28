#include <cinolib/gl/glcanvas.h>
#include <cinolib/gl/surface_mesh_controls.h>
#include <cinolib/drawable_triangle_soup.h>
#include <thread>
#include "booleans.h"

int main(int argc, char **argv)
{
    BoolOp op = UNION;
    int vert_offset = 0;
    std::vector<std::string> files = {"../data/bunny.obj", "../data/cow.obj"};

    std::vector<double> in_coords, bool_coords;
    std::vector<uint> in_tris, bool_tris;
    std::vector<uint> in_labels;
    std::vector<std::bitset<NBIT>> bool_labels;

    std::vector<double>            back_coords;
    std::vector<uint>              back_tris;
    std::vector<std::bitset<NBIT>> back_labels;

    std::cout << "Commands:" << std::endl;
    std::cout << "- press I for Intersection" << std::endl;
    std::cout << "- press U for Union" << std::endl;
    std::cout << "- press S for Subtraction" << std::endl;
    std::cout << "- press Space for Pause/Resume" << std::endl;

    loadMultipleFiles(files, in_coords, in_tris, in_labels, vert_offset);

    const cinolib::Color & c0 = cinolib::Color::PASTEL_ORANGE();
    const cinolib::Color & c1 = cinolib::Color::PASTEL_CYAN();
    uint n_tri = bool_tris.size()/3;
    std::vector<cinolib::Color> tri_colors(n_tri, c0);

    for(uint id = 0; id < n_tri; ++id)
        if(bool_labels[id][1])
            tri_colors[id] = c1;

    for(uint i = vert_offset; i < in_coords.size(); ++i)
        in_coords[i] += 1e-5;

    cinolib::DrawableTriangleSoup soup(bool_coords, bool_tris, tri_colors);
    cinolib::GLcanvas gui;
    gui.push(&soup);

    std::mutex mutex;

    std::atomic<bool> pause = false;
    gui.callback_key_pressed = [&](int key, int mod) -> bool
    {
        if(key==GLFW_KEY_I) op = INTERSECTION; else
        if(key==GLFW_KEY_U) op = UNION;        else
        if(key==GLFW_KEY_S) op = SUBTRACTION;  else
        if(key==GLFW_KEY_SPACE) pause = !pause; else
        if(key==GLFW_KEY_W)
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::vector<cinolib::Color> colors(bool_tris.size()/3);
            for(uint i=0; i<colors.size(); ++i)
            {
                colors[i] = (bool_labels[i][1]) ? cinolib::Color::PASTEL_ORANGE() : cinolib::Color::PASTEL_CYAN();
            }
        }
        return false;
    };

    // boolean thread
    std::atomic<bool> done = false;
    std::atomic<bool> exit = false;
    std::atomic<int> count = 0;
    std::thread boolean_thread([&]()
   {
       while(!exit)
       {
           back_coords.clear();
           back_tris.clear();
           booleanPipeline(in_coords, in_tris, in_labels, op, back_coords, back_tris, back_labels);
           {
               std::lock_guard<std::mutex> lock(mutex);
               back_coords.swap(bool_coords);
               back_tris.swap(bool_tris);
               back_labels.swap(bool_labels);
           }
           done = true; // next frame is ready to render
           ++count;

           if(exit) return;

           if(!pause) // rotate
           {
               static cinolib::mat3d R = cinolib::mat3d::ROT_3D(cinolib::vec3d(0,1,0), cinolib::to_rad(3));
               for(uint i=vert_offset; i<in_coords.size(); i+=3)
               {
                   cinolib::vec3d p = R * cinolib::vec3d(in_coords[i], in_coords[i+1], in_coords[i+2]);
                   in_coords[i  ] = p[0];
                   in_coords[i+1] = p[1];
                   in_coords[i+2] = p[2];
               }
           }
       }
   });

   std::promise<void> fps_exit_signal;
   std::thread fps_thread([&]()
   {
       std::future<void> exit_condition = fps_exit_signal.get_future();
       while(exit_condition.wait_for(std::chrono::milliseconds(1000)) == std::future_status::timeout)
       {
           //std::cout << "fps: " << count << std::endl;
           count = 0;
       }
   });

    // ui thread
    glfwMakeContextCurrent(gui.window);
    while(!glfwWindowShouldClose(gui.window))
    {
        if(done)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                n_tri = bool_tris.size()/3;
                tri_colors.resize(n_tri, c0);
                for(uint id=0; id<n_tri; ++id)
                    tri_colors[id] = (bool_labels[id][0]) ? c0 : c1;
                soup = cinolib::DrawableTriangleSoup(bool_coords, bool_tris, tri_colors);
            }
            done = false;
        }
        gui.draw();
        glfwPollEvents();
    }

    exit = true;
    fps_exit_signal.set_value();
    if(boolean_thread.joinable()) boolean_thread.join();
    fps_thread.join();
    return EXIT_SUCCESS;
}