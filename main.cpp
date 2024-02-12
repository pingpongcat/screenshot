#include <cairo.h>
#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>


int main()
{
	int fd = open("/dev/dri/card2", O_RDWR); // Open the DRM device
	if (fd < 0)
	{
		std::cerr << "Failed to open DRM device\n";
		return -1;
	}

	drmModeRes *resources = drmModeGetResources(fd);
	if (!resources)
	{
		std::cerr << "Failed to get DRM resources\n";
		close(fd);
		return -1;
	}

	// Debugging loop to log all connectors and their status
	for (int i = 0; i < resources->count_connectors; ++i)
	{
		drmModeConnector *conn = drmModeGetConnector(fd, resources->connectors[i]);
		if (!conn)
			continue;

		std::cout << "Connector " << conn->connector_id
				  << " type: " << conn->connector_type
				  << " status: " << (conn->connection == DRM_MODE_CONNECTED ? "Connected" : "Disconnected")
				  << " modes: " << conn->count_modes << std::endl;

		drmModeFreeConnector(conn);
	}

	drmModeConnector *connectedConnector = nullptr;
	for (int i = 0; i < resources->count_connectors; ++i)
	{
		drmModeConnector *conn = drmModeGetConnector(fd, resources->connectors[i]);
		if (conn && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0)
		{
			connectedConnector = conn;
			break; // Found the first active connector
		}
		drmModeFreeConnector(conn); // Free the connector if it's not used
	}

	if (!connectedConnector)
	{
		std::cerr << "Failed to find a connected connector with modes.\n";
		drmModeFreeResources(resources);
		close(fd);
		return -1;
	}

	drmModeModeInfo *mode = &connectedConnector->modes[0]; 

	uint32_t fb_id;
	uint32_t width = mode->hdisplay;
	uint32_t height = mode->vdisplay;
	uint32_t bpp = 32; // Bits per pixel
	uint32_t stride;
	uint32_t handle;
	int ret;

	// Create a dumb buffer
	struct drm_mode_create_dumb create_dumb = {0};
	create_dumb.width = width;
	create_dumb.height = height;
	create_dumb.bpp = bpp;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
	if (ret < 0)
	{
		std::cerr << "Failed to create dumb buffer\n";
		return -1;
	}

	stride = create_dumb.pitch;
	handle = create_dumb.handle;

	// Create the framebuffer object
	ret = drmModeAddFB(fd, width, height, 24, bpp, stride, handle, &fb_id);
	if (ret)
	{
		std::cerr << "Failed to add framebuffer\n";
		return -1;
	}

	struct drm_mode_map_dumb mreq;
	void *mapped_buffer;

	// Prepare the buffer for mapping
	memset(&mreq, 0, sizeof(mreq));
	mreq.handle = handle; // 'handle' is obtained from the previous drmModeAddFB or create_dumb call

	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq))
	{
		std::cerr << "Failed to prepare buffer for mapping (DRM_IOCTL_MODE_MAP_DUMB)\n";
		return -1;
	}

	// Perform the actual mapping
	mapped_buffer = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
	if (mapped_buffer == MAP_FAILED)
	{
		std::cerr << "Failed to mmap framebuffer: " << strerror(errno) << "\n";
		return -1;
	}

	// Saving the image with libjpeg
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *outfile;
	const char *filename = "framebuffer.jpeg";
	int row_stride; // Width of the row in bytes

	size_t buffer_size;	 // Size of the buffer, calculated as height * stride

	// Initialize the JPEG compression object
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	// Open file for writing
	if ((outfile = fopen(filename, "wb")) == NULL)
	{
		fprintf(stderr, "Cannot open %s for writing\n", filename);
		exit(1);
	}
	jpeg_stdio_dest(&cinfo, outfile);

	// Set image parameters
	cinfo.image_width = width; // Image width and height, in pixels
	cinfo.image_height = height;
	cinfo.input_components = 3;		// Here we assume we're dealing with an RGB framebuffer
	cinfo.in_color_space = JCS_RGB; // JCS_RGB for RGB color space

	// Set default compression parameters
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 75, TRUE); // Adjust the quality as needed

	// Start compression
	jpeg_start_compress(&cinfo, TRUE);

	row_stride = width * 3; // Assuming 3 bytes per pixel (RGB)

	while (cinfo.next_scanline < cinfo.image_height)
	{
		// Calculate the pointer to the beginning of the current row in the mapped buffer
		JSAMPROW row_pointer[1]; // Array of a single row's start address
		row_pointer[0] = &((JSAMPLE *)mapped_buffer)[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	// Finish compression and clean up
	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);

	// Assuming the buffer_size is correctly calculated as height * stride earlier
	munmap(mapped_buffer, buffer_size); // Unmap the framebuffer memory

	drmModeRmFB(fd, fb_id); // Remove framebuffer
	struct drm_mode_destroy_dumb destroy_dumb = {0};
	destroy_dumb.handle = handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb); // Destroy dumb buffer
	drmModeFreeConnector(connectedConnector);				  // Free the connector

	drmModeFreeResources(resources);
	close(fd);
	return 0;
}
