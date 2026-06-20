#include "application/AGatorApplication.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <fstream>

namespace {
	std::ofstream g_logFile;

	void LogOutput(const std::string& message) {
		if (g_logFile.is_open()) {
			g_logFile << message;
			g_logFile.flush();
		}
		std::cout << message;
	}

	void ConfigureDebugLog() {
		const std::filesystem::path logPath = std::filesystem::current_path() / "navigator-debug.txt";

		try {
			g_logFile.open(logPath, std::ios::out | std::ios::trunc);
			if (!g_logFile.is_open()) {
				std::cerr << "Warning: Could not open debug log at " << logPath.string() << std::endl;
				return;
			}

			LogOutput("=== NaviGator debug session ===\n");
			LogOutput("Working directory: " + std::filesystem::current_path().string() + "\n");

			std::set_terminate([]() {
				try {
					if (const auto currentException = std::current_exception()) {
						std::rethrow_exception(currentException);
					}
				}
				catch (const std::exception& ex) {
					LogOutput(std::string("Unhandled exception: ") + ex.what() + "\n");
				}
				catch (...) {
					LogOutput("Unhandled non-standard exception\n");
				}

				if (g_logFile.is_open()) {
					g_logFile.close();
				}
			});
		}
		catch (const std::exception& ex) {
			std::cerr << "Error configuring debug log: " << ex.what() << std::endl;
		}
	}
}

int main(int argc, char* argv[]) {
	ConfigureDebugLog();

	AGatorApplication app;

	if (!app.Setup()) {
		std::cout << "Failed to set up Gator, please contact Gamma!" << std::endl;
		return 0;
	}

	app.Run();

	if (!app.Teardown()) {
		std::cout << "Something went wrong on teardown, please contact Gamma!" << std::endl;
		return 0;
	}
}
