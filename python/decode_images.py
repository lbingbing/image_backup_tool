import os
import concurrent.futures
import PIL.Image

import decode_image

def worker(image_file_path, *args):
    with PIL.Image.open(image_file_path) as img:
        return decode_image.decode_image(img, *args)

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('part_dir_path', help='part dir path')
    parser.add_argument('image_dir_path', help='image dir path')
    parser.add_argument('dim', help='dim as row_num,col_num')
    transform_utils.add_transform_arguments(parser)
    parser.add_argument('--mp', type=int, default=1, help='multiprocessing')
    args = parser.parse_args()

    row_num, col_num = decode_image.parse_dim(args.dim)
    transform = transform_utils.get_transform(args)

    image_file_names = sorted(os.listdir(args.image_dir_path))
    image_file_paths = [os.path.join(args.image_dir_path, e) for e in image_file_names]
    total_num = len(image_file_paths)
    success_part_ids = set()
    success_part_num = 0
    success_num = 0
    fail_num = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.mp) as executor:
        futures = {executor.submit(worker, image_file_path, row_num, col_num, transform): image_file_name for image_file_name, image_file_path in zip(image_file_names, image_file_paths)}
        for done, future in enumerate(concurrent.futures.as_completed(futures), 1):
            image_file_name = futures[future]
            success, part_id, part_bytes, result_img = future.result()
            if success:
                print('{}/{} {} success part_id {}'.format(done, total_num, image_file_name, part_id))
                part_file_path = os.path.join(args.part_dir_path, '{}'.format(part_id))
                if part_id not in success_part_ids and not os.path.isfile(part_file_path):
                    with open(part_file_path, 'wb') as f:
                        f.write(part_bytes)
                    success_part_ids.add(part_id)
                    success_part_num += 1
                success_num += 1
            else:
                print('{}/{} {} fail'.format(done, total_num, image_file_name))
                fail_num += 1
    print('success_part_num', success_part_num)
    print('success_num', success_num)
    print('fail_num', fail_num)
