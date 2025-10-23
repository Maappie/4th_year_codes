class Api::V1::MessagesController < Api::V1::BaseController
  before_action :authenticate_device!

  def create
    msg = Message.new(msg_params)
    if msg.save
      render json: {
        id: msg.id,
        sender_tag: msg.sender_tag,
        message: msg.message,
        nonce: msg.nonce,
        created_at: msg.created_at.iso8601
      }, status: :created
    else
      render json: { errors: msg.errors.full_messages }, status: :unprocessable_entity
    end
  end

  private
  def msg_params
    params.permit(:sender_tag, :message, :nonce)
  end
end
