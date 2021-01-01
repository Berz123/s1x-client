#include <std_include.hpp>
#include "loader/component_loader.hpp"
//#include "system_check.hpp"
//#include "scheduler.hpp"

#include "game/game.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>
#include <utils/string.hpp>
#include <utils/thread.hpp>
#include <utils/compression.hpp>

//#include <exception/minidump.hpp>

#include <version.hpp>

namespace exception
{
	namespace
	{
		thread_local struct
		{
			DWORD code = 0;
			PVOID address = nullptr;
		} exception_data;

		struct
		{
			std::chrono::time_point<std::chrono::high_resolution_clock> last_recovery{};
			std::atomic<int> recovery_counts = {0};
		} recovery_data;

		bool is_exception_interval_too_short()
		{
			const auto delta = std::chrono::high_resolution_clock::now() - recovery_data.last_recovery;
			return delta < 1min;
		}

		bool too_many_exceptions_occured()
		{
			return recovery_data.recovery_counts >= 3;
		}

		volatile bool& is_initialized()
		{
			static volatile bool initialized = false;
			return initialized;
		}

		void show_mouse_cursor()
		{
			while (ShowCursor(TRUE) < 0);
		}

		void display_error_dialog()
		{
			std::string error_str = utils::string::va("Fatal error (0x%08X) at 0x%p.\n\n",
			                                          exception_data.code, exception_data.address);

			error_str += "Make sure to update your graphics card drivers and install operating system updates!";

			utils::thread::suspend_other_threads();
			show_mouse_cursor();

			MessageBoxA(nullptr, error_str.data(), "ERROR", MB_ICONERROR);
			TerminateProcess(GetCurrentProcess(), exception_data.code);
		}

		std::string get_timestamp()
		{
			tm ltime{};
			char timestamp[MAX_PATH] = {0};
			const auto time = _time64(nullptr);

			_localtime64_s(&ltime, &time);
			strftime(timestamp, sizeof(timestamp) - 1, "%Y-%m-%d-%H-%M-%S", &ltime);

			return timestamp;
		}

		bool is_harmless_error(const LPEXCEPTION_POINTERS exceptioninfo)
		{
			const auto code = exceptioninfo->ExceptionRecord->ExceptionCode;
			return code == STATUS_INTEGER_OVERFLOW || code == STATUS_FLOAT_OVERFLOW || code == STATUS_SINGLE_STEP;
		}

		LONG WINAPI exception_filter(const LPEXCEPTION_POINTERS exceptioninfo)
		{
			if (!is_harmless_error(exceptioninfo))
			{
				exception_data.code = exceptioninfo->ExceptionRecord->ExceptionCode;
				exception_data.address = exceptioninfo->ExceptionRecord->ExceptionAddress;
				display_error_dialog();
			}

			return EXCEPTION_CONTINUE_EXECUTION;
		}

		LPTOP_LEVEL_EXCEPTION_FILTER WINAPI set_unhandled_exception_filter_stub(LPTOP_LEVEL_EXCEPTION_FILTER)
		{
			// Don't register anything here...
			return &exception_filter;
		}
	}

	class component final : public component_interface
	{
	public:
		void post_load() override
		{
			SetUnhandledExceptionFilter(exception_filter);
			utils::hook::jump(SetUnhandledExceptionFilter, set_unhandled_exception_filter_stub, true);
		}
	};
}

REGISTER_COMPONENT(exception::component)
