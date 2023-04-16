//
//  main.c
//  SystemProcessorsTray
//
//  Created by Caio Cozza on 4/15/23.
//

#include <Carbon/Carbon.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/vm_statistics.h>
#include <mach/mach.h>

double memory_total_double;

static OSStatus AppEventHandler(EventHandlerCallRef inCaller, EventRef inEvent, void* inRefcon) {
	return noErr;
}

void total_cpus(void *menu) {
	int mib[2];
	int num_processors;
	size_t length = sizeof(num_processors);
	char title[64];
	
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	
	if(sysctl(mib, 2, &num_processors, &length, NULL, 0) == -1) {
		perror("total_cpus:sysctl");
		return;
	}
	
	snprintf(title, sizeof(title), "Total Processors: %d", num_processors);
	SetMenuItemTextWithCFString(menu, 1, CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8));
}

void total_ram(void *menu) {
	int mib[2];
	unsigned long memory_total;
	size_t length = sizeof(memory_total);
	char title[64];
	
	mib[0] = CTL_HW;
	mib[1] = HW_PHYSMEM;
	
	if (sysctl(mib, 2, &memory_total, &length, NULL, 0) == -1) {
		perror("total_ram:sysctl");
		return;
	}
	
	memory_total_double = ((double) memory_total) / (1024 * 1024);
	
	snprintf(title, sizeof(title), "Memory Total: %.2f mb", memory_total_double);
	SetMenuItemTextWithCFString(menu, 2, CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8));
}

void basic_stats(void *menu) {
	total_cpus(menu);
	total_ram(menu);
}

static void memory_timer_callback(EventLoopTimerRef inTimer, void *menu) {
	mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
	vm_statistics_data_t vmstat;
	kern_return_t ret;
	mach_port_t host_port = mach_host_self();
	char title[64];
	
	ret = host_statistics(host_port, HOST_VM_INFO, (host_info_t)&vmstat, &count);
	
	if (ret != KERN_SUCCESS) {
		perror("memory_timer_callback:host_statistics");
		return;
	}
	
	unsigned long free_long = vmstat.free_count * vm_page_size;
	double free_double = (double)free_long / (1024*1024);
	
	snprintf(title, sizeof(title), "Memory Free: %.2f mb", free_double);
	SetMenuItemTextWithCFString(menu, 3, CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8));
	
	snprintf(title, sizeof(title), "Memory Used: %.2f mb", memory_total_double - free_double);
	SetMenuItemTextWithCFString(menu, 4, CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8));
}

void register_menu_item(unsigned int index, void *menu) {
	MenuItemIndex menu_item;
	CFStringRef item_text = CFStringCreateWithCString(NULL, "", kCFStringEncodingUTF8);
	AppendMenuItemTextWithCFString(menu, item_text, 0, index, &menu_item);
	CFRelease(item_text);
}

void register_menu_item_loop(uint8_t size, void *menu) {
	uint8_t index;
	for (index = 0; index < size; index++) {
		register_menu_item(index, menu);
	}
}

int main(int argc, char* argv[]) {
	EventTypeSpec event_spec = { kEventClassApplication, kEventAppFrontSwitched };
	InstallApplicationEventHandler(NewEventHandlerUPP(AppEventHandler), 1, &event_spec, NULL, NULL);
	
	MenuRef menu = GetMenuHandle(0);
	if (!menu) {
		CFStringRef menu_title = CFStringCreateWithCString(NULL, "System Monitor", kCFStringEncodingUTF8);
		CreateNewMenu(0,0,&menu);
		SetMenuTitleWithCFString(menu, menu_title);
		CFRelease(menu_title);
		InsertMenu(menu, 0);
	}
	
	register_menu_item_loop(4, menu);

	IconRef icon_ref;
	ControlRef status_item_control;
	OSStatus status = GetIconRef(kOnSystemDisk, kSystemIconsCreator, kGenericApplicationIcon, &icon_ref);
	if (status != noErr) {
		fprintf(stderr, "Failed to create icon control.\n");
		return 1;
	}
	
	SetControlPopupMenuHandle(status_item_control, menu);
	SetControlVisibility(status_item_control, true, false);
	
	basic_stats(menu);
	
	EventLoopTimerRef timer;
	EventLoopTimerUPP memory_timer_UPP = NewEventLoopTimerUPP(memory_timer_callback);
	
	InstallEventLoopTimer(GetMainEventLoop(), 0, 30 * kEventDurationSecond, memory_timer_UPP, menu, &timer);
	
	RunApplicationEventLoop();
	DisposeEventLoopTimerUPP(memory_timer_UPP);
	return 0;
}