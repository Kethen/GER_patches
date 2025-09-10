IMAGE_NAME=gbe_log_printer

set -xe

if [ "$REBUILD_IMAGE" == "true" ] && podman image exists $IMAGE_NAME
then
    podman image rm --force "$IMAGE_NAME"
fi

if ! podman image exists $IMAGE_NAME
then
	podman image build -t $IMAGE_NAME -f DockerFile
fi

podman run \
	--rm -it \
	--security-opt label=disable \
	-v ./:/work_dir \
	-w /work_dir \
	--entrypoint /bin/bash \
	$IMAGE_NAME \
	build.sh
