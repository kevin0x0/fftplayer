
$(OBJ_DIR)/fft.o : $(SRC_DIR)/fft.c $(INC_DIR)/fft.h | create_dir
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/minimp3-minimp3.o : $(SRC_DIR)/minimp3/minimp3.c $(INC_DIR)/minimp3/minimp3.h $(INC_DIR)/minimp3/minimp3_ex.h $(INC_DIR)/minimp3/minimp3.h | create_dir
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/glad.o : $(SRC_DIR)/glad.c $(INC_DIR)/glad/glad.h $(INC_DIR)/KHR/khrplatform.h | create_dir
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/audio.o : $(SRC_DIR)/audio.c $(INC_DIR)/audio.h $(INC_DIR)/minimp3/minimp3.h | create_dir
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/main.o : $(SRC_DIR)/main.c $(INC_DIR)/GLFW/glfw3.h $(INC_DIR)/glad/glad.h $(INC_DIR)/KHR/khrplatform.h $(INC_DIR)/minimp3/minimp3.h $(INC_DIR)/minimp3/minimp3_ex.h $(INC_DIR)/minimp3/minimp3.h $(INC_DIR)/audio.h $(INC_DIR)/minimp3/minimp3.h $(INC_DIR)/fft.h | create_dir
	$(CC) $(CFLAGS) -c -o $@ $<

