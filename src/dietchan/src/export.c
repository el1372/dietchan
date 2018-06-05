#include "export.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <libowfat/uint16.h>
#include <libowfat/uint32.h>
#include <libowfat/uint64.h>
#include <libowfat/str.h>
#include <libowfat/textcode.h>
#include "persistence.h"

void print_esc(const char *s)
{
	if (!s) return;
	char *buf = alloca(strlen(s)*6+1);
	buf[fmt_jsonescape(buf, s, strlen(s))]= '\0';
	printf("%s", buf);
}

void export()
{
	char buf[256];
	printf("{\n");
	printf("  \"boards\": [\n");
	for (struct board *board=master_first_board(master); board; board=board_next_board(board)) {
		printf("    {\n");
		printf("      \"id\": %" PRIu64 ",\n", board_id(board));
		printf("      \"name\": \""); print_esc(board_name(board)); printf("\",\n");
		printf("      \"title\": \""); print_esc(board_title(board)); printf("\",\n");
		printf("      \"threads\": [\n");

		for (struct thread *thread=board_first_thread(board); thread; thread=thread_next_thread(thread)) {
			printf("        {\n");
			printf("          \"closed\": %d,\n", (int)thread_closed(thread));
			printf("          \"pinned\": %d,\n", (int)thread_pinned(thread));
			printf("          \"saged\": %d,\n",  (int)thread_saged(thread));
			printf("          \"posts\": [\n");
			for (struct post *post=thread_first_post(thread); post; post=post_next_post(post)) {
				printf("            {\n");
				printf("              \"id\": %" PRIu64 ",\n", post_id(post));
				buf[fmt_ip(buf, &post_ip(post))] = '\0';
				printf("              \"ip\": \"%s\",\n", buf);
				buf[fmt_ip(buf, &post_x_real_ip(post))] = '\0';
				printf("              \"x_real_ip\": \"%s\",\n", buf);
				if (post_x_forwarded_for(post)) {
					printf("              \"x_forwarded_for\": [", buf);
					for (uint64 i=0; i<post_x_forwarded_for_count(post); ++i) {
						buf[fmt_ip(buf, &post_x_forwarded_for(post)[i])] = '\0';
						printf("\"%s\"%s", buf, i<post_x_forwarded_for_count(post)-1?", ":"");
					}
					printf("],\n"); // /x_forwarded_for
				}
				printf("              \"useragent\": \""); print_esc(post_useragent(post)); printf("\",\n");
				printf("              \"user_role\": %d,\n", (int)post_user_role(post));
				printf("              \"username\": \""); print_esc(post_username(post)); printf("\",\n");
				printf("              \"password\": \""); print_esc(post_password(post)); printf("\",\n");
				printf("              \"sage\": %d,\n", (int)post_sage(post));
				printf("              \"banned\": %d,\n", (int)post_banned(post));
				printf("              \"reported\": %d,\n", (int)post_banned(post));
				printf("              \"ban_message\": \""); print_esc(post_ban_message(post)); printf("\",\n");

				printf("              \"timestamp\": %" PRIu64 ",\n", post_timestamp(post));
				printf("              \"subject\": \""); print_esc(post_subject(post)); printf("\",\n");
				printf("              \"text\": \""); print_esc(post_text(post)); printf("\",\n");
				printf("              \"uploads\": [\n");
				for (struct upload *up = post_first_upload(post); up; up=upload_next_upload(up)) {
					printf("                {\n");
					printf("                  \"file\": \""); print_esc(upload_file(up)); printf("\",\n");
					printf("                  \"thumbnail\": \""); print_esc(upload_thumbnail(up)); printf("\",\n");
					printf("                  \"original_name\": \""); print_esc(upload_original_name(up)); printf("\",\n");
					printf("                  \"mime_type\": \""); print_esc(upload_mime_type(up)); printf("\",\n");
					printf("                  \"size\": %" PRIu64 ",\n", upload_size(up));
					printf("                  \"width\": %" PRId32 ",\n", upload_width(up));
					printf("                  \"height\": %" PRId32 ",\n", upload_height(up));
					printf("                  \"duration\": %" PRId64 ",\n", upload_duration(up));
					printf("                  \"state\": %" PRId32 "\n", upload_state(up));
					printf("                }%s\n", upload_next_upload(up)?",":"");
				}
				printf("              ]\n"); // /uploads
				printf("            }%s\n", post_next_post(post)?",":""); // /post

			}
			printf("          ]\n"); // /posts
			printf("        }%s\n", thread_next_thread(thread)?",":""); // /thread
		}
		printf("      ]\n"); // /threads
		printf("    }%s\n", board_next_board(board)?",":""); // /board
	}
	printf("  ],\n"); // /boards

	printf("  \"users\": [\n");
	for (struct user *user=master_first_user(master); user; user=user_next_user(user)) {
		printf("    {\n");
		printf("      \"id\": %" PRIu64 ",\n", user_id(user));
		printf("      \"name\": \""); print_esc(user_name(user)); printf("\",\n");
		printf("      \"password\": \""); print_esc(user_password(user)); printf("\",\n");
		printf("      \"email\": \""); print_esc(user_email(user)); printf("\",\n");
		printf("      \"type\": %d", (int)user_type(user));

		int64 *bids = user_boards(user);
		if (bids) {
			printf(",\n");
			printf("      \"boards\": [");
			for (int64 *bid=bids; *bid != -1; ++bid)
				printf("%" PRIu64 "%s", *bid, bid[1]!=-1?", ":"");
			printf("]\n"); // /boards
		} else {
			printf("\n");
		}
		printf("    }%s\n", user_next_user(user)?",":""); // /user
	}
	printf("  ]\n"); // /users
	printf("}");
}
