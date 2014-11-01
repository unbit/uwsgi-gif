#include <uwsgi.h>

extern struct uwsgi_server uwsgi;

static int commit_line(struct uwsgi_buffer *ub, uint8_t *line, uint8_t line_pos) {
	if (uwsgi_buffer_u8(ub, line_pos)) return -1;
	if (uwsgi_buffer_append(ub, (char *) line, line_pos)) return -1;
	return 0;
}

static int push_pixel(struct uwsgi_buffer *ub, uint8_t *line, uint8_t *line_pos, uint8_t *bits_used, uint8_t value) {
	switch(*bits_used) {
		case 0:
			line[*line_pos] = value;	
			*bits_used = 3;
			break;
		case 1:
			line[*line_pos] |= (value << 1);
			*bits_used = 4;
			break;
		case 2:
			line[*line_pos] |= (value << 2);
			*bits_used = 5;
			break;
		case 3:
			line[*line_pos] |= (value << 3);
			*bits_used = 6;
			break;
		case 4:
			line[*line_pos] |= (value << 4);
			*bits_used = 7;
			break;
		case 5:
			line[*line_pos] |= (value << 5);
			// end of a byte
			if ((*line_pos) + 1 >= 255) {
				if (commit_line(ub, line, (*line_pos)+1)) return -1;
				*line_pos = 0;
			}
			else {
				*line_pos += 1;
			}
			*bits_used = 0; 
			break;
		case 6:
			line[*line_pos] |= (value << 6);
			// end of a byte
			if ((*line_pos) + 1 >= 255) {
				if (commit_line(ub, line, (*line_pos)+1)) return -1;
				*line_pos = 0;
			}
			else {
				*line_pos += 1;
			}
			line[*line_pos] = value >> 2;
			*bits_used = 1;
			break;
		case 7:
			line[*line_pos] |= (value << 7);
			// end of a byte
			if ((*line_pos) + 1 >= 255) {
				if (commit_line(ub, line, (*line_pos)+1)) return -1;
				*line_pos = 0;
			}
			else {
				*line_pos += 1;
			}
			line[*line_pos] = value >> 1;
			*bits_used = 2;
			break;
		default:
			return -1;
	}

	return 0;

}

static int fill_gif(struct uwsgi_buffer *ub, uint32_t pixels) {
	uint32_t i;
	uint8_t line[255];
	uint8_t line_pos = 0;
	uint8_t bits_used = 0;

	for(i=0;i<pixels+1;i++) {
		// end code
		if (i >= pixels) {
			if (push_pixel(ub, line, &line_pos, &bits_used, 5)) return -1;
		}
		else {
			if (i % 2 == 0) {
				if (push_pixel(ub, line, &line_pos, &bits_used, 4)) return -1;
			}
			if (push_pixel(ub, line, &line_pos, &bits_used, 1)) return -1;
		}
	}

	if (line_pos > 0 || bits_used > 0) {
                if (commit_line(ub, line, line_pos+1)) return -1;
        }

        return 0;
}

static int gif_generator(struct uwsgi_buffer *ub, uint16_t width, uint16_t height, uint8_t red, uint8_t green, uint8_t blue) {
	// header
	if (uwsgi_buffer_append(ub, "GIF89a", 6)) return -1;
	// width
	if (uwsgi_buffer_u16le(ub, width)) return -1;
	// height
	if (uwsgi_buffer_u16le(ub, height)) return -1;
	// Global Color Table Flag (GCTF) + background index (0) + aspect ratio (0)
	if (uwsgi_buffer_append(ub, "\x80\x00\x00", 3)) return -1;
	// Color 0 (white)
	if (uwsgi_buffer_append(ub, "\xff\xff\xff", 3)) return -1;

	// Color 1
	if (uwsgi_buffer_u8(ub, red)) return -1;
	if (uwsgi_buffer_u8(ub, green)) return -1;
	if (uwsgi_buffer_u8(ub, blue)) return -1;


	// image separator (0x2c) + left position (0) + top position (0) 
	if (uwsgi_buffer_append(ub, "\x2c\0\0\0\0", 5)) return -1;
	// width
        if (uwsgi_buffer_u16le(ub, width)) return -1;
        // height
        if (uwsgi_buffer_u16le(ub, height)) return -1;
	// no local color table nor interlaced +  lzw minimal number of items/codes (2)
	if (uwsgi_buffer_append(ub, "\0\x02", 2)) return -1;

	uint32_t pixels = width * height;

	// now spit out the image LZW
	if (fill_gif(ub, pixels)) return -1;

	if (uwsgi_buffer_append(ub, "\0\x3b", 2)) return -1;

	return 0;
	
	
}

static int router_gif_func(struct wsgi_request *wsgi_req, struct uwsgi_route *ur) {

	char *width_str = NULL;
	char *height_str = NULL;
	char *red_str = NULL;
	char *green_str = NULL;
	char *blue_str = NULL;

	uint16_t width = 1;
	uint16_t height = 1;
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;

	char **subject = (char **) (((char *)(wsgi_req))+ur->subject);
        uint16_t *subject_len = (uint16_t *)  (((char *)(wsgi_req))+ur->subject_len);

        struct uwsgi_buffer *ub = uwsgi_routing_translate(wsgi_req, ur, *subject, *subject_len, ur->data, ur->data_len);
        if (!ub) return UWSGI_ROUTE_BREAK;

        if (uwsgi_kvlist_parse(ub->buf, ub->pos, ',', '=',
                        "width", &width_str,
                        "w", &width_str,
                        "height", &height_str,
                        "h", &height_str,
                        "red", &red_str,
                        "r", &red_str,
                        "green", &green_str,
                        "g", &green_str,
                        "blue", &blue_str,
                        "blu", &blue_str,
                        "b", &blue_str,
                        NULL)) {
        	uwsgi_log("[gif] unable to parse options\n");
		goto end;
        }

	if (width_str && *width_str != 0) width = atoi(width_str);
	if (height_str && *height_str != 0) height = atoi(height_str);
	if (red_str && *red_str != 0) red = atoi(red_str);
	if (green_str && *green_str != 0) green = atoi(green_str);
	if (blue_str && *blue_str != 0) blue = atoi(blue_str);

	if (uwsgi_response_prepare_headers(wsgi_req, "200 OK", 6)) goto end;
	if (uwsgi_response_add_content_type(wsgi_req, "image/gif", 9)) goto end;

	struct uwsgi_buffer *ub_gif = uwsgi_buffer_new(uwsgi.page_size);
	if (gif_generator(ub_gif, width, height, red, green, blue)) goto end;
	uwsgi_response_write_body_do(wsgi_req, ub_gif->buf, ub_gif->pos);
	uwsgi_buffer_destroy(ub_gif);

end:
	uwsgi_buffer_destroy(ub);
	if (width_str) free(width_str);
	if (height_str) free(height_str);
	if (red_str) free(red_str);
	if (green_str) free(green_str);
	if (blue_str) free(blue_str);
	return UWSGI_ROUTE_BREAK;
}

static int router_gif(struct uwsgi_route *ur, char *args) {
	ur->func = router_gif_func;
        ur->data = args;
        ur->data_len = strlen(args);
        return 0;
}


static void register_gif_router() {
	uwsgi_register_router("gif", router_gif);
}

struct uwsgi_plugin gif_plugin = {
	.name = "gif",
	.on_load = register_gif_router,
};
