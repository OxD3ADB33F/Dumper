#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define COLOR "\x1b[32m"
#define RESET "\x1b[0m"

void 
mem_read (pid_t pid) 
{
  char p_map[64];
  sprintf(p_map, "/proc/%d/maps", pid);

  FILE *f_map = fopen(p_map, "r");
  if (f_map == NULL) 
    {
      perror("fopen");
      exit(EXIT_FAILURE);
    }

  char line[256];
  while (fgets(line, sizeof(line), f_map)) 
    {
      printf("%s", line);
    }

  fclose(f_map);
}

void 
mem_dump (pid_t pid, const char *pattern) 
{
  char mem_path[64];
  sprintf(mem_path, "/proc/%d/mem", pid);

  int mem_fd = open(mem_path, O_RDONLY);
  if (mem_fd == -1) 
    {
      perror("open");
      exit(EXIT_FAILURE);
    }

  FILE *f_map;
  char p_map[64];
  sprintf(p_map, "/proc/%d/maps", pid);

  f_map = fopen(p_map, "r");
  if (!f_map)
    {
      perror("fopen");
      close(mem_fd);
      exit(EXIT_FAILURE);
    }

  char line[256];
  while (fgets(line, sizeof(line), f_map)) 
    {
      unsigned long start, end;
      if (sscanf(line, "%lx-%lx", &start, &end) == 2) 
        {
          if (start > 0x00007fffffffffff) 
            {
              continue;
            }

          lseek(mem_fd, start, SEEK_SET);

          size_t region_size = end - start;
          size_t chunk_size = 4096;
          char *buffer = malloc(chunk_size);

          for (size_t offset = 0; offset < region_size; offset += chunk_size) 
            {
              size_t btr = (region_size - offset < chunk_size) ? region_size - offset : chunk_size;

              ssize_t result = pread(mem_fd, buffer, btr, start + offset);
              if (result == -1) 
                {
                  if (errno == EIO) 
                    {
                      break;
                    } 
                  else if (errno == EINVAL) 
                    {
                      break;
                    } 
                  else 
                    {
                      perror("pread");
                      free(buffer);
                      fclose(f_map);
                      close(mem_fd);
                      exit(EXIT_FAILURE);
                    }
                }

              if (memmem(buffer, btr, pattern, strlen(pattern))) 
                {
                  printf(COLOR "[+] " RESET "Pattern '%s' found in memory region: %lx-%lx\n", pattern, start, end);
                  break;
                }
            }

          free(buffer);
        }
    }

    fclose(f_map);
    close(mem_fd);
}

int
main (int argc, char *argv[]) 
{
  if (argc != 3) 
    {
      fprintf(stderr, "Usage: %s <pid> <pattern>\n", argv[0]);
      return EXIT_FAILURE;
    }

  pid_t pid = atoi(argv[1]);
  const char *pattern = argv[2];

  printf(COLOR "[+] " RESET "Reading memory maps of PID %d:\n\n", pid);
  mem_read(pid);

  printf(COLOR "\n[+] " RESET "Dumping memory of PID %d for pattern \"%s\":\n\n", pid, pattern);
  mem_dump(pid, pattern);

  return EXIT_SUCCESS;
}
