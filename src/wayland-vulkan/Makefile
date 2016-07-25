WAYLAND_VULKAN_PROTOCOLS = wayland-vulkan-protocol.xml
CLIENT_SO=wayland-vulkan-client.so
SERVER_SO=wayland-vulkan-server.so

wayland-vulkan-protocol.c : $(WAYLAND_VULKAN_PROTOCOLS)
	wayland-scanner code < $< > $@

wayland-vulkan-server-protocol.h : $(WAYLAND_VULKAN_PROTOCOLS)
	wayland-scanner server-header < $< > $@

wayland-vulkan-client-protocol.h : $(WAYLAND_VULKAN_PROTOCOLS)
	wayland-scanner client-header < $< > $@

.DEFAULT_GOAL = all
all : $(CLIENT_SO) $(SERVER_SO)

$(CLIENT_SO): wayland-vulkan-protocol.c wayland-vulkan-client-protocol.h
	$(CC) -shared -o wayland-vulkan-client.so wayland-vulkan-protocol.c wayland-vulkan-client-protocol.h

$(SERVER_SO): wayland-vulkan-protocol.c wayland-vulkan-server-protocol.h
	$(CC) -shared -o wayland-vulkan-server.so wayland-vulkan-protocol.c wayland-vulkan-server-protocol.h

clean:
	rm -rf $(CLIENT_SO) $(SERVER_SO) *.c *h
