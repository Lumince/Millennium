#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h> // Must include before gl.h
#include <stdio.h>
#include <regex>
#include <sstream>
#include "font.h"


static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

class FastAnsiParser {
private:
    static constexpr std::array<ImU32, 16> ANSI_COLORS = {
        IM_COL32(40, 40, 40, 255),      // 0: Black (slightly brighter)
        IM_COL32(220, 80, 80, 255),     // 1: Dark Red (brighter)
        IM_COL32(80, 220, 80, 255),     // 2: Dark Green (brighter)
        IM_COL32(220, 220, 80, 255),    // 3: Dark Yellow (brighter)
        IM_COL32(80, 80, 220, 255),     // 4: Dark Blue (brighter)
        IM_COL32(220, 80, 220, 255),    // 5: Dark Magenta (brighter)
        IM_COL32(80, 220, 220, 255),    // 6: Dark Cyan (brighter)
        IM_COL32(220, 220, 220, 255),   // 7: Light Gray (brighter)
        IM_COL32(160, 160, 160, 255),   // 8: Dark Gray (brighter)
        IM_COL32(255, 120, 120, 255),   // 9: Bright Red (enhanced)
        IM_COL32(120, 255, 120, 255),   // 10: Bright Green (enhanced)
        IM_COL32(255, 255, 120, 255),   // 11: Bright Yellow (enhanced)
        IM_COL32(120, 120, 255, 255),   // 12: Bright Blue (enhanced)
        IM_COL32(255, 120, 255, 255),   // 13: Bright Magenta (enhanced)
        IM_COL32(120, 255, 255, 255),   // 14: Bright Cyan (enhanced)
        IM_COL32(255, 255, 255, 255)    // 15: White
    };
    
    static constexpr ImU32 DEFAULT_COLOR = IM_COL32(255, 255, 255, 255);
    
    struct TextSegment {
        const char* text;
        size_t length;
        ImU32 color;
        bool bold;
    };
    
    // Fast integer parsing - avoids atoi overhead
    static inline int parseAnsiCode(const char*& ptr, const char* end) {
        int result = 0;
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            result = result * 10 + (*ptr - '0');
            ++ptr;
        }
        return result;
    }
    
    // Convert 256-color ANSI to RGB
    static ImU32 ansi256ToRgb(int code) {
        if (code < 16) {
            return ANSI_COLORS[code];
        } else if (code < 232) {
            // 216 color cube
            code -= 16;
            int r = (code / 36) * 51;
            int g = ((code % 36) / 6) * 51;
            int b = (code % 6) * 51;
            return IM_COL32(r, g, b, 255);
        } else {
            // Grayscale
            int gray = 8 + (code - 232) * 10;
            return IM_COL32(gray, gray, gray, 255);
        }
    }

public:
    static void RenderAnsiText(const char* text, size_t length = 0) {
        if (!text) return;
        
        if (length == 0) length = strlen(text);
        
        const char* ptr = text;
        const char* end = text + length;
        const char* segmentStart = ptr;
        
        ImU32 currentColor = DEFAULT_COLOR;
        bool currentBold = false;
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 startPos = ImGui::GetCursorScreenPos();
        ImVec2 pos = startPos;
        float lineHeight = ImGui::GetTextLineHeight();
        float availableWidth = ImGui::GetContentRegionAvail().x;
        
        auto renderTextSegment = [&](const char* segStart, const char* segEnd) {
            while (segStart < segEnd) {
                const char* current = segStart;
                
                // Process character by character to handle control chars
                while (current < segEnd) {
                    if (*current == '\n') {
                        // Render text before newline
                        if (current > segStart) {
                            drawList->AddText(pos, currentColor, segStart, current);
                            pos.x += ImGui::CalcTextSize(segStart, current).x;
                        }
                        // Move to next line
                        pos.x = startPos.x;
                        pos.y += lineHeight;
                        segStart = current + 1;
                        current = segStart;
                        continue;
                    } else if (*current == '\r') {
                        // Render text before carriage return
                        if (current > segStart) {
                            drawList->AddText(pos, currentColor, segStart, current);
                            pos.x += ImGui::CalcTextSize(segStart, current).x;
                        }
                        // Carriage return - move cursor to start of current line
                        pos.x = startPos.x;
                        segStart = current + 1;
                        current = segStart;
                        continue;
                    } else if (*current == '\t') {
                        // Render text before tab
                        if (current > segStart) {
                            drawList->AddText(pos, currentColor, segStart, current);
                            pos.x += ImGui::CalcTextSize(segStart, current).x;
                        }
                        // Tab - move to next tab stop (8 characters)
                        float charWidth = ImGui::CalcTextSize("A").x;
                        float tabWidth = charWidth * 8;
                        float currentColumn = pos.x - startPos.x;
                        float nextTabStop = ((int)(currentColumn / tabWidth) + 1) * tabWidth;
                        pos.x = startPos.x + nextTabStop;
                        segStart = current + 1;
                        current = segStart;
                        continue;
                    } else if (*current == '\b') {
                        // Render text before backspace
                        if (current > segStart) {
                            drawList->AddText(pos, currentColor, segStart, current);
                            pos.x += ImGui::CalcTextSize(segStart, current).x;
                        }
                        // Backspace - move cursor back one character
                        float charWidth = ImGui::CalcTextSize("A").x;
                        pos.x = std::max(startPos.x, pos.x - charWidth);
                        segStart = current + 1;
                        current = segStart;
                        continue;
                    } else if ((unsigned char)*current < 32 && *current != '\033') {
                        // Skip other control characters (except ESC which we handle separately)
                        if (current > segStart) {
                            drawList->AddText(pos, currentColor, segStart, current);
                            pos.x += ImGui::CalcTextSize(segStart, current).x;
                        }
                        segStart = current + 1;
                        current = segStart;
                        continue;
                    }
                    current++;
                }
                
                // Render any remaining text
                if (current > segStart) {
                    drawList->AddText(pos, currentColor, segStart, current);
                    pos.x += ImGui::CalcTextSize(segStart, current).x;
                }
                break;
            }
        };
        
        while (ptr < end) {
            if (*ptr == '\033' && ptr + 1 < end && *(ptr + 1) == '[') {
                // Render text segment before escape sequence
                if (ptr > segmentStart) {
                    renderTextSegment(segmentStart, ptr);
                }
                
                // Parse ANSI escape sequence
                ptr += 2; // Skip '\033['
                
                // Parse codes separated by semicolons
                while (ptr < end && *ptr != 'm') {
                    int code = parseAnsiCode(ptr, end);
                    
                    if (code == 0) {
                        // Reset
                        currentColor = DEFAULT_COLOR;
                        currentBold = false;
                    } else if (code == 1) {
                        // Bold
                        currentBold = true;
                    } else if (code == 22) {
                        // Normal intensity
                        currentBold = false;
                    } else if (code >= 30 && code <= 37) {
                        // Standard foreground colors
                        currentColor = ANSI_COLORS[code - 30];
                    } else if (code >= 90 && code <= 97) {
                        // Bright foreground colors  
                        currentColor = ANSI_COLORS[code - 90 + 8];
                    } else if (code == 38 && ptr < end && *ptr == ';') {
                        // 256-color or RGB foreground
                        ++ptr; // Skip ';'
                        int type = parseAnsiCode(ptr, end);
                        if (type == 5 && ptr < end && *ptr == ';') {
                            // 256-color
                            ++ptr; // Skip ';'
                            int colorCode = parseAnsiCode(ptr, end);
                            currentColor = ansi256ToRgb(colorCode);
                        } else if (type == 2) {
                            // RGB
                            if (ptr < end && *ptr == ';') {
                                ++ptr;
                                int r = parseAnsiCode(ptr, end);
                                if (ptr < end && *ptr == ';') {
                                    ++ptr;
                                    int g = parseAnsiCode(ptr, end);
                                    if (ptr < end && *ptr == ';') {
                                        ++ptr;
                                        int b = parseAnsiCode(ptr, end);
                                        currentColor = IM_COL32(r, g, b, 255);
                                    }
                                }
                            }
                        }
                    }
                    
                    // Skip semicolon
                    if (ptr < end && *ptr == ';') ++ptr;
                }
                
                // Skip 'm'
                if (ptr < end && *ptr == 'm') ++ptr;
                
                segmentStart = ptr;
            } else {
                ++ptr;
            }
        }
        
        // Render final segment
        if (ptr > segmentStart) {
            renderTextSegment(segmentStart, ptr);
        }
        
        // Update cursor position - handle special case if text ends with \r\n or \n
        if (length > 0) {
            if (text[length - 1] == '\n') {
                pos.x = startPos.x;
                if (length == 1 || text[length - 2] != '\r') {
                    pos.y += lineHeight;
                }
            } else if (text[length - 1] == '\r') {
                pos.x = startPos.x;
            }
        }
        ImGui::SetCursorScreenPos(pos);
    }
};

// Convenience function
void RenderAnsiText(const char* text) {
    FastAnsiParser::RenderAnsiText(text);
}

void RenderAnsiText(const std::string& text) {
    FastAnsiParser::RenderAnsiText(text.c_str(), text.length());
}

int show_console()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(5, 5, "Millennium Debug Console", nullptr, nullptr);
    if (!window)
        return 1;


    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync


    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    float XDPI, YDPI;
    glfwGetWindowContentScale(window, &XDPI, &YDPI);

    io.Fonts->AddFontFromMemoryTTF((void*)SauceCodeProNerdFont_Regular, sizeof(SauceCodeProNerdFont_Regular), 16.0f * XDPI);
    io.DisplayFramebufferScale = ImVec2(XDPI, XDPI);

    /** Reset current ImGui style */
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(XDPI);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        for (const auto& logger : g_loggerList)
        {
            std::string strLogs;
            ImGui::Begin(logger->GetPluginName(false).c_str());
            auto logs = logger->CollectLogs();

            for (const auto& log : logs)
            {
                strLogs += log.message;
            }

            RenderAnsiText(strLogs.c_str());

            ImGui::End();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}