from io import BytesIO


def tobytes(img, format='TIFF'):
    buf = BytesIO()
    img.save(buf, format)

    return buf.getvalue()
