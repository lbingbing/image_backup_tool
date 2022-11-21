set(file_to_install
    pixel_codec.py
    test_pixel_codec.py
    test_pixel_codec_python_c.py
    decode_image_task.py
    decode_image_task_status_client.py
    display_qt.py
)

install(FILES ${file_to_install} DESTINATION bin)
