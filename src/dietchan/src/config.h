#ifndef CONFIG_H
#define CONFIG_H

// -- Convenience --
#define KILO (1024UL)
#define MEGA (1024UL*1024UL)
#define GIGA (1024UL*1024UL*1024UL)

// -- Flood limits --

// Number of seconds to wait between creating two posts (seconds)
//#define FLOOD_LIMIT                    10
#define FLOOD_LIMIT                       0
// Number of seconds to wait between creating two reports (seconds)
#define REPORT_FLOOD_LIMIT               10

// -- Boards --

// Number of preview posts for thread in board view
#define PREVIEW_REPLIES                   4
// Number of threads per page in board view
#define THREADS_PER_PAGE                 10
// Maximum number of pages per board
#define MAX_PAGES                        16

// -- Threads --

// Thread will go on autosage after this many posts
#define BUMP_LIMIT                      100
// Absolute limit of how many posts a thread can have. When reaching this limit, it is automatically
// closed.
#define POST_LIMIT                     1000

// -- Posts --
#define POST_MAX_BODY_LENGTH          10000
#define POST_MAX_SUBJECT_LENGTH         100
#define POST_MAX_NAME_LENGTH            100

// -- Uploads --
// Maximum filename length of an uploaded file
#define MAX_FILENAME_LENGTH             512
// Maximum length of a mime type
#define MAX_MIME_LENGTH                  64
// Maximum number of files attached to a post
#define MAX_FILES_PER_POST                4
// Maximum file size of a single upload
#define MAX_UPLOAD_SIZE           (10*MEGA)

// -- Reports --
#define REPORT_MAX_COMMENT_LENGTH       100

// -- Technical definitions -- CAREFUL! --

// When changing these definitions, please note that some strings are allocated on the stack, so
// these constants should not be too big.
// Also, the purpose of these constants is to prevent errors or attacks. As such, they are just
// rough guidelines. The code may not always follow to these constants to the byte, but will stay
// within the right order of magnitude.
#define MAX_HEADER_LENGTH              2048
#define MAX_URL_LENGTH                 2048
#define MAX_REQUEST_LINE_LENGTH        2048
#define MAX_GET_PARAM_LENGTH           2048
#define MAX_POST_PARAM_LENGTH         16384
#define MAX_MULTIPART_BOUNDARY_LENGTH   128

#endif // CONFIG_H
